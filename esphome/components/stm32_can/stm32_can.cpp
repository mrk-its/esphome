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
// void STM32Can::loop() {
//   auto fifo0_cnt = HAL_CAN_GetRxFifoFillLevel(&hcan, 0);
//   auto fifo1_cnt = HAL_CAN_GetRxFifoFillLevel(&hcan, 1);
//   if(fifo1_cnt) {
//     ESP_LOGE(TAG, "not expected message in fifo #1");
//   }

//   CAN_RxHeaderTypeDef header;
//   uint8_t data[8];

//   if(fifo0_cnt) {
//     if(HAL_CAN_GetRxMessage(&hcan, 0, &header, data) == HAL_OK) {
//       ESP_LOGI(TAG, "fifo #0, received msg from %d, dlc: %d", header.StdId, header.DLC);
//     }
//   }
// }

bool STM32Can::setup_internal() {
  CAN_FilterTypeDef filter = {0};
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // for 125kbit/s
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 40;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
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
    ESP_LOGI(TAG, "CAN: Initialized, state: %d, err: %d", hcan.State, hcan.ErrorCode);

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
  ESP_LOGI(TAG, "free tx buffers: %ld", free_tx);
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