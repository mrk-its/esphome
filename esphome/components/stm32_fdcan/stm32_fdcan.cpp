#ifdef USE_STM32

#include "esphome.h"
#include "stm32_fdcan.h"

extern "C" {
FDCAN_HandleTypeDef *hcan_ptr[3] = {nullptr, nullptr, nullptr};
}

namespace esphome {
namespace stm32_fdcan {

static std::vector<STM32FDCan *> fdcan_instances;

void STM32FDCan::setup() {
  ESP_LOGCONFIG(TAG, "Setting up STM32FDCan");
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

bool STM32FDCan::setup_internal() {
  if (!fdcan_instances.capacity()) {
    fdcan_instances.reserve(4);
  }
  fdcan_instances.push_back(this);

  FDCAN_FilterTypeDef filter = {0};
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN1;
  PeriphClkInit.Fdcan1ClockSelection = RCC_FDCAN1CLKSOURCE_PLL1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler();
  }
  __HAL_RCC_FDCAN1_CLK_ENABLE();

  tx_pin_->setup();
  rx_pin_->setup();

  if (hcan_.Instance == FDCAN1) {
    hcan_ptr[0] = &hcan_;
  }
#ifdef FDCAN2
  if (hcan_.Instance == FDCAN2) {
    hcan_ptr[1] = &hcan_;
  }
#endif
#ifdef FDCAN3
  if (hcan_.Instance == FDCAN3) {
    hcan_ptr[2] = &hcan_;
  }
#endif

  hcan_.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hcan_.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hcan_.Init.Mode = FDCAN_MODE_NORMAL;
  hcan_.Init.AutoRetransmission = DISABLE;
  hcan_.Init.TransmitPause = DISABLE;
  hcan_.Init.ProtocolException = DISABLE;
  hcan_.Init.StdFiltersNbr = 0;
  hcan_.Init.ExtFiltersNbr = 0;
  hcan_.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;

  // TODO - determine PLL1_FREQ runtime, from clock config
  const uint32_t PLL1_FREQ = 80000000;
  uint32_t bitrate = CAN_BITRATES[bit_rate_];

  for (uint32_t prescaller = 1; prescaller <= 512; prescaller++) {
    if ((PLL1_FREQ % (bitrate * prescaller)) == 0) {
      uint32_t ticks_per_bit = PLL1_FREQ / prescaller / bitrate;
      uint32_t tseg1 = ticks_per_bit * 3 / 4;
      uint32_t tseg2 = ticks_per_bit - tseg1 - 1;
      if (tseg1 >= 2 && tseg1 < 256 && tseg2 >= 2 && tseg2 <= 128) {
        ESP_LOGCONFIG(TAG, "prescaller: %lu, tseg1: %lu, tseg2: %lu", prescaller, tseg1, tseg2);
        hcan_.Init.NominalPrescaler = prescaller;  // 1..512
        hcan_.Init.NominalSyncJumpWidth = 1;       // 1..128
        hcan_.Init.NominalTimeSeg1 = tseg1;        // 2..256
        hcan_.Init.NominalTimeSeg2 = tseg2;        // 2..128
        break;
      } else {
        ESP_LOGV(TAG, "prescaller: %lu, tseg1: %lu, tseg2: %lu - out of range", prescaller, tseg1, tseg2);
      }
    }
  }

  if (!hcan_.Init.NominalPrescaler) {
    ESP_LOGE(TAG, "cannot compute timings for %ld pll1 freq and %ld bitrate", PLL1_FREQ, bitrate);
    this->mark_failed();
    return false;
  }

  // RCC_OscInitTypeDef osc_config;

  // HAL_RCC_GetOscConfig(&osc_config);
  // ESP_LOGI(TAG, "Oscilator config retrieved, PLL1Q: %d", osc_config.PLL.PLLQ);

  HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

  if (HAL_FDCAN_Init(&hcan_) == HAL_OK) {
    // TODO: make sure ErrorCode is 0 and State is READY

    ESP_LOGI(TAG, "FDCAN: Initialized, state: %d, err: %lu", hcan_.State, hcan_.ErrorCode);

    if (HAL_FDCAN_ActivateNotification(&hcan_, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) == HAL_OK) {
      ESP_LOGI(TAG, "rxfifo0 interrupt activated");
    } else {
      ESP_LOGE(TAG, "can't activate rxfifo0 interrupt");
    }

    if (HAL_FDCAN_ConfigGlobalFilter(&hcan_, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT, FDCAN_REJECT_REMOTE,
                                     FDCAN_REJECT_REMOTE) != HAL_OK) {
      ESP_LOGE(TAG, "Can't configure global filter");
    } else {
      ESP_LOGI(TAG, "global filter configured");
    }

    if (HAL_FDCAN_Start(&hcan_) == HAL_OK) {
      ESP_LOGI(TAG, "FDCAN: Started");
    }

    if (on_initialized_) {
      on_initialized_->trigger();
    }

    return true;
  }
  return false;
}
static uint32_t last_millis = 0;

void STM32FDCan::loop() {
  canbus::Canbus::loop();
  return;
  uint32_t t = millis();
  if (t - last_millis < 1000) {
    return;
  }
  last_millis = t;
  uint32_t fill_rx0 = HAL_FDCAN_GetRxFifoFillLevel(&hcan_, FDCAN_RX_FIFO0);
  uint32_t fill_rx1 = HAL_FDCAN_GetRxFifoFillLevel(&hcan_, FDCAN_RX_FIFO1);
  FDCAN_ErrorCountersTypeDef cnts = {0};
  FDCAN_ProtocolStatusTypeDef status = {0};
  HAL_FDCAN_GetErrorCounters(&hcan_, &cnts);
  HAL_FDCAN_GetProtocolStatus(&hcan_, &status);

  ESP_LOGV(
      TAG,
      "rx fill[0: %lu, 1: %lu], errors[tx: %lu, rx: %lu, rx_passive: %lu, cnt: %lu], activity: %lx, err_passive: %ld, "
      "warn: %lu, bus_off: %lu, last_err: %lu",
      fill_rx0, fill_rx1, cnts.TxErrorCnt, cnts.RxErrorCnt, cnts.RxErrorPassive, cnts.ErrorLogging, status.Activity,
      status.ErrorPassive, status.Warning, status.BusOff, status.LastErrorCode);
}

canbus::Error STM32FDCan::send_message(struct canbus::CanFrame *frame) {
  FDCAN_TxHeaderTypeDef TxHeader = {0};

  TxHeader.Identifier = frame->can_id;
  TxHeader.IdType = frame->use_extended_id ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
  TxHeader.TxFrameType = frame->remote_transmission_request ? FDCAN_REMOTE_FRAME : FDCAN_DATA_FRAME;
  TxHeader.DataLength = frame->can_data_length_code;
  TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
  TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;  // FDCAN_STORE_TX_EVENTS;  // FDCAN_NO_TX_EVENTS
  TxHeader.MessageMarker = 0;

  uint32_t free_tx = HAL_FDCAN_GetTxFifoFreeLevel(&hcan_);

  if (!free_tx) {
    ESP_LOGE(TAG, "output queues are full");
    return canbus::ERROR_ALLTXBUSY;
  }

  ESP_LOGV(TAG, "free tx buffers: %ld", free_tx);

  if (HAL_FDCAN_AddMessageToTxFifoQ(&hcan_, &TxHeader, frame->data) == HAL_OK) {
    ESP_LOGV(TAG, "message sent, state: %d, err: %ld", hcan.State, hcan.ErrorCode);
  } else {
    ESP_LOGE(TAG, "can't send message");
  }
  return canbus::ERROR_OK;
};

const uint32_t RX_FIFO_IDS[] = {FDCAN_RX_FIFO0, FDCAN_RX_FIFO1};
void STM32FDCan::push_can_frame(FDCAN_HandleTypeDef *hcan, struct canbus::CanFrame *frame) {
  if (&hcan_ == hcan) {
    if (!rx_fifo.add(frame)) {
      ESP_LOGW(TAG, "rx_fifo is full");
    }
  }
}

canbus::Error STM32FDCan::read_message(struct canbus::CanFrame *frame) {
  return rx_fifo.get(frame) ? canbus::ERROR_OK : canbus::ERROR_NOMSG;
};

extern "C" void FDCAN1_IT0_IRQHandler(void) {
  if (hcan_ptr[0]) {
    HAL_FDCAN_IRQHandler(hcan_ptr[0]);
  }
}

#ifdef FDCAN2
extern "C" void FDCAN2_IT0_IRQHandler(void) {
  if (hcan_ptr[1]) {
    HAL_FDCAN_IRQHandler(hcan_ptr[1]);
  }
}
#endif

#ifdef FDCAN3
extern "C" void FDCAN3_IT0_IRQHandler(void) {
  if (hcan_ptr[2]) {
    HAL_FDCAN_IRQHandler(hcan_ptr[2]);
  }
}
#endif

extern "C" void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs) {
  // translate fdcan ptr to CANBus instance
  // and call on_new_frame on it (or so)
  if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) {
    FDCAN_RxHeaderTypeDef header;
    esphome::canbus::CanFrame frame;
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &header, frame.data) == HAL_OK) {
      frame.can_id = header.Identifier;
      frame.use_extended_id = (header.IdType == FDCAN_EXTENDED_ID);
      frame.remote_transmission_request = (header.RxFrameType == FDCAN_REMOTE_FRAME);
      frame.can_data_length_code = header.DataLength;
      for (auto it = fdcan_instances.begin(); it != fdcan_instances.end(); it++) {
        (*it)->push_can_frame(hfdcan, &frame);
      }
    }
  }
}

}  // namespace stm32_fdcan
}  // namespace esphome

#endif
