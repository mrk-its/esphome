#ifdef USE_STM32

#include "esphome.h"
#include "stm32_can.h"

namespace esphome {
namespace stm32_can {

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
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  hcan.Instance = CAN1;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;

  uint32_t bitrate = CAN_BITRATES[bit_rate_];
  uint32_t sys_freq = HAL_RCC_GetSysClockFreq();

  if (sys_freq % bitrate) {
    ESP_LOGE(TAG, "sys_freq (%lu Hz) must be a multiply of requested bitrate (%lu bps)", sys_freq, bitrate);
    return false;
  }
  uint32_t prescaller_tq = sys_freq / bitrate;
  if (!(prescaller_tq % 16)) {
    hcan.Init.Prescaler = prescaller_tq / 16;
    hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
  } else if (!(prescaller_tq % 8)) {
    hcan.Init.Prescaler = prescaller_tq / 8;
    hcan.Init.TimeSeg1 = CAN_BS1_6TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_1TQ;
  } else if (!(prescaller_tq % 4)) {
    hcan.Init.Prescaler = prescaller_tq / 4;
    hcan.Init.TimeSeg1 = CAN_BS1_2TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_1TQ;
  } else {
    ESP_LOGE(TAG, "cannot setup timings for sys_freq (%lu Hz) and bitrate (%lu bps)", sys_freq, bitrate);
    return false;
  }
  ESP_LOGCONFIG(TAG, "prescaller: %lu, tseg1: %lx, tseg2: %lx", hcan.Init.Prescaler, hcan.Init.TimeSeg1,
                hcan.Init.TimeSeg2);

  HAL_CAN_MspInit(&hcan);
  __HAL_RCC_CAN1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  if (HAL_CAN_Init(&hcan) == HAL_OK) {
    // TODO: make sure ErrorCode is 0 and State is READY
    ESP_LOGI(TAG, "CAN: Initialized, state: %d, err: %lu", hcan.State, hcan.ErrorCode);

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

    if (HAL_CAN_ConfigFilter(&hcan, &filter) == HAL_OK) {
      ESP_LOGI(TAG, "CAN: filter configured");
    }
    if (HAL_CAN_Start(&hcan) == HAL_OK) {
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

  TxHeader.StdId = frame->can_id;
  TxHeader.IDE = frame->use_extended_id ? CAN_ID_EXT : CAN_ID_STD;
  TxHeader.RTR = frame->remote_transmission_request ? CAN_RTR_REMOTE : CAN_RTR_DATA;
  TxHeader.DLC = frame->can_data_length_code;

  uint32_t free_tx = HAL_CAN_GetTxMailboxesFreeLevel(&hcan);
  if (!free_tx) {
    ESP_LOGE(TAG, "output queues are full");
    return canbus::ERROR_ALLTXBUSY;
  }
  ESP_LOGV(TAG, "free tx buffers: %ld", free_tx);
  if (HAL_CAN_AddTxMessage(&hcan, &TxHeader, frame->data, &TxMailbox) == HAL_OK) {
    ESP_LOGV(TAG, "message sent, state: %ld, err: %ld", hcan.State, hcan.ErrorCode);
  } else {
    ESP_LOGE(TAG, "can't send message");
  }

  return canbus::ERROR_OK;
};

canbus::Error STM32Can::read_message(struct canbus::CanFrame *frame) {
  auto fifo0_cnt = HAL_CAN_GetRxFifoFillLevel(&hcan, 0);
  auto fifo1_cnt = HAL_CAN_GetRxFifoFillLevel(&hcan, 1);
  if (fifo1_cnt) {
    ESP_LOGE(TAG, "not expected message in fifo #1");
  }

  CAN_RxHeaderTypeDef header;

  if (fifo0_cnt && (HAL_CAN_GetRxMessage(&hcan, 0, &header, frame->data) == HAL_OK)) {
    frame->can_id = header.StdId;
    frame->use_extended_id = (header.IDE == CAN_ID_EXT);
    frame->remote_transmission_request = (header.RTR == CAN_RTR_REMOTE);
    frame->can_data_length_code = header.DLC;
    ESP_LOGV(TAG, "fifo #0, received msg from %d, dlc: %d", header.StdId, header.DLC);
    return canbus::ERROR_OK;
  }

  uint32_t err = HAL_CAN_GetError(&hcan);
  if (err) {
    ESP_LOGD(TAG, "err: %ld", err);
  }

  return canbus::ERROR_NOMSG;
};

}  // namespace stm32_can
}  // namespace esphome
#endif
