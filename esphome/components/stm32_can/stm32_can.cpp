#ifdef USE_STM32

#include "esphome.h"
#include "stm32_can.h"

extern "C" {
CAN_HandleTypeDef *hcan_ptr[2] = {nullptr, nullptr};
}

namespace esphome {
namespace stm32_can {

static std::vector<STM32Can *> can_instances;

void STM32Can::setup() {
  ESP_LOGCONFIG(TAG, "Setting up STM32Can");
  if (!this->setup_internal()) {
    ESP_LOGE(TAG, "setup error!");
    this->mark_failed();
  }
}

const uint32_t CAN_BITRATES[] = {
    1000,  5000,  10000, 12500,  16000,  20000,  25000,  31250,  33000,  40000,   50000,
    80000, 83300, 95000, 100000, 125000, 200000, 250000, 500000, 800000, 1000000,
};

static_assert((canbus::CanSpeed::CAN_1000KBPS + 1 == sizeof(CAN_BITRATES) / 4));

bool STM32Can::setup_internal() {
  CAN_FilterTypeDef filter = {0};

  if (!can_instances.capacity()) {
    can_instances.reserve(4);
  }
  can_instances.push_back(this);

  auto pcan = &this->hcan_;

  if (hcan_.Instance == CAN1) {
    hcan_ptr[0] = pcan;
  }
#ifdef CAN2
  if (hcan_.Instance == CAN2) {
    hcan_ptr[1] = pcan;
  }
#endif

  pcan->Init.Mode = CAN_MODE_NORMAL;
  pcan->Init.SyncJumpWidth = CAN_SJW_1TQ;
  pcan->Init.TimeTriggeredMode = DISABLE;
  pcan->Init.AutoBusOff = ENABLE;
  pcan->Init.AutoWakeUp = ENABLE;
  pcan->Init.AutoRetransmission = ENABLE;
  pcan->Init.ReceiveFifoLocked = DISABLE;
  pcan->Init.TransmitFifoPriority = DISABLE;

  uint32_t bitrate = CAN_BITRATES[bit_rate_];
  uint32_t pclk1_freq = HAL_RCC_GetPCLK1Freq();

  if (pclk1_freq % bitrate) {
    ESP_LOGE(TAG, "PCLK1 frequency (%lu Hz) must be a multiply of requested bitrate (%lu bps)", pclk1_freq, bitrate);
    return false;
  }
  uint32_t prescaller_tq = pclk1_freq / bitrate;
  if (!(prescaller_tq % 16)) {
    pcan->Init.Prescaler = prescaller_tq / 16;
    pcan->Init.TimeSeg1 = CAN_BS1_13TQ;
    pcan->Init.TimeSeg2 = CAN_BS2_2TQ;
  } else if (!(prescaller_tq % 8)) {
    pcan->Init.Prescaler = prescaller_tq / 8;
    pcan->Init.TimeSeg1 = CAN_BS1_6TQ;
    pcan->Init.TimeSeg2 = CAN_BS2_1TQ;
  } else if (!(prescaller_tq % 4)) {
    pcan->Init.Prescaler = prescaller_tq / 4;
    pcan->Init.TimeSeg1 = CAN_BS1_2TQ;
    pcan->Init.TimeSeg2 = CAN_BS2_1TQ;
  } else {
    ESP_LOGE(TAG, "cannot setup CAN timings for PCLK1 frequency: %lu Hz and bitrate: %lu bps", pclk1_freq, bitrate);
    return false;
  }
  ESP_LOGCONFIG(TAG, "prescaller: %lu, tseg1: %lx, tseg2: %lx", pcan->Init.Prescaler, pcan->Init.TimeSeg1,
                pcan->Init.TimeSeg2);

  HAL_CAN_MspInit(pcan);
  __HAL_RCC_CAN1_CLK_ENABLE();

  HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);

  this->tx_pin_->setup();
  this->rx_pin_->setup();

  if (HAL_CAN_Init(pcan) == HAL_OK) {
    // TODO: make sure ErrorCode is 0 and State is READY
    ESP_LOGI(TAG, "CAN: Initialized, state: %d, err: %lu", pcan->State, pcan->ErrorCode);

    filter.FilterActivation = CAN_FILTER_ENABLE;
    filter.FilterScale = CAN_FILTERSCALE_16BIT;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterIdHigh = 0;
    filter.FilterIdLow = 0;
    filter.FilterMaskIdHigh = 0;
    filter.FilterMaskIdLow = 0;
    filter.FilterFIFOAssignment = 0;
    filter.FilterBank = 0;
    filter.SlaveStartFilterBank = 0;

    if (HAL_CAN_ConfigFilter(pcan, &filter) == HAL_OK) {
      ESP_LOGI(TAG, "CAN: filter configured");
    }
    if (HAL_CAN_Start(pcan) == HAL_OK) {
      ESP_LOGI(TAG, "CAN: Started");
    } else {
      ESP_LOGE(TAG, "CAN: Can't start");
    }
    return true;
  }
  return false;
}

canbus::Error STM32Can::send_message(struct canbus::CanFrame *frame) {
  CAN_TxHeaderTypeDef TxHeader;
  uint32_t TxMailbox = 0;
  auto pcan = &this->hcan_;

  TxHeader.StdId = frame->can_id;
  TxHeader.IDE = frame->use_extended_id ? CAN_ID_EXT : CAN_ID_STD;
  TxHeader.RTR = frame->remote_transmission_request ? CAN_RTR_REMOTE : CAN_RTR_DATA;
  TxHeader.DLC = frame->can_data_length_code;

  uint32_t free_tx = HAL_CAN_GetTxMailboxesFreeLevel(pcan);
  if (!free_tx) {
    ESP_LOGE(TAG, "output queues are full");
    return canbus::ERROR_ALLTXBUSY;
  }
  ESP_LOGV(TAG, "free tx buffers: %ld", free_tx);
  if (HAL_CAN_AddTxMessage(pcan, &TxHeader, frame->data, &TxMailbox) == HAL_OK) {
    ESP_LOGV(TAG, "message sent, state: %ld, err: %ld", pcan->State, pcan->ErrorCode);
  } else {
    ESP_LOGE(TAG, "can't send message");
  }

  return canbus::ERROR_OK;
};

void STM32Can::push_can_frame(CAN_HandleTypeDef *hcan, struct canbus::CanFrame *frame) {
  if (&hcan_ == hcan) {
    if (!rx_fifo_->add(frame)) {
      ESP_LOGW(TAG, "rx_fifo is full");
    }
  }
}

canbus::Error STM32Can::read_message(struct canbus::CanFrame *frame) {
  return rx_fifo_->get(frame) ? canbus::ERROR_OK : canbus::ERROR_NOMSG;
};

extern "C" void CAN1_IT0_IRQHandler(void) {
  if (hcan_ptr[0]) {
    HAL_CAN_IRQHandler(hcan_ptr[0]);
  }
}

#ifdef CAN2
extern "C" void CAN2_IT0_IRQHandler(void) {
  if (hcan_ptr[1]) {
    HAL_CAN_IRQHandler(hcan_ptr[1]);
  }
}
#endif

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  CAN_RxHeaderTypeDef header;
  esphome::canbus::CanFrame frame;

  if (HAL_CAN_GetRxMessage(hcan, 0, &header, frame.data) == HAL_OK) {
    frame.can_id = header.StdId;
    frame.use_extended_id = (header.IDE == CAN_ID_EXT);
    frame.remote_transmission_request = (header.RTR == CAN_RTR_REMOTE);
    frame.can_data_length_code = header.DLC;
    ESP_LOGV(TAG, "fifo #0, received msg from %d, dlc: %d", header.StdId, header.DLC);
    for (auto it = can_instances.begin(); it != can_instances.end(); it++) {
      (*it)->push_can_frame(hcan, &frame);
    }
  }
}

}  // namespace stm32_can
}  // namespace esphome
#endif
