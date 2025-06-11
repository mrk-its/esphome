#ifdef USE_STM32

#include "esphome.h"
#include "stm32_fdcan.h"

namespace esphome {
namespace stm32_fdcan {

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

  hcan.Instance = FDCAN1;
  hcan.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hcan.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hcan.Init.Mode = FDCAN_MODE_NORMAL;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.TransmitPause = DISABLE;
  hcan.Init.ProtocolException = DISABLE;
  hcan.Init.NominalPrescaler = 16;
  hcan.Init.NominalSyncJumpWidth = 1;
  hcan.Init.NominalTimeSeg1 = 30;
  hcan.Init.NominalTimeSeg2 = 9;
  hcan.Init.DataPrescaler = 16;
  hcan.Init.DataSyncJumpWidth = 1;
  hcan.Init.DataTimeSeg1 = 30;
  hcan.Init.DataTimeSeg2 = 9;
  hcan.Init.StdFiltersNbr = 1;
  hcan.Init.ExtFiltersNbr = 0;
  hcan.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;

  // uint32_t bitrate = CAN_BITRATES[bit_rate_];
  // uint32_t pclk1_freq = HAL_RCC_GetSYSCLKFreq();

  // if (pclk1_freq % bitrate) {
  //   ESP_LOGE(TAG, "PCLK1 frequency (%lu Hz) must be a multiply of requested bitrate (%lu bps)", pclk1_freq, bitrate);
  //   return false;
  // }
  // uint32_t prescaller_tq = pclk1_freq / bitrate;
  // if (!(prescaller_tq % 16)) {
  //   hcan.Init.NominalPrescaler = prescaller_tq / 16;
  //   hcan.Init.NominalTimeSeg1 = CAN_BS1_13TQ;
  //   hcan.Init.NominalTimeSeg2 = CAN_BS2_2TQ;
  // } else if (!(prescaller_tq % 8)) {
  //   hcan.Init.NominalPrescaler = prescaller_tq / 8;
  //   hcan.Init.NominalTimeSeg1 = CAN_BS1_6TQ;
  //   hcan.Init.NominalTimeSeg2 = CAN_BS2_1TQ;
  // } else if (!(prescaller_tq % 4)) {
  //   hcan.Init.NominalPrescaler = prescaller_tq / 4;
  //   hcan.Init.NominalTimeSeg1 = CAN_BS1_2TQ;
  //   hcan.Init.NominalTimeSeg2 = CAN_BS2_1TQ;
  // } else {
  //   ESP_LOGE(TAG, "cannot setup CAN timings for PCLK1 frequency: %lu Hz and bitrate: %lu bps", pclk1_freq, bitrate);
  //   return false;
  // }
  // ESP_LOGCONFIG(TAG, "prescaller: %lu, tseg1: %lx, tseg2: %lx", hcan.Init.Prescaler, hcan.Init.TimeSeg1,
  //               hcan.Init.TimeSeg2);

  // RCC_OscInitTypeDef osc_config;

  // HAL_RCC_GetOscConfig(&osc_config);
  // ESP_LOGI(TAG, "Oscilator config retrieved, PLL1Q: %d", osc_config.PLL.PLLQ);

  if (HAL_FDCAN_Init(&hcan) == HAL_OK) {
    // TODO: make sure ErrorCode is 0 and State is READY

    ESP_LOGI(TAG, "can memory size: %d", hcan.msgRam.TxFIFOQSA - hcan.msgRam.StandardFilterSA + 3 * (18U * 4U));

    ESP_LOGI(TAG, "FDCAN: Initialized, state: %d, err: %lu", hcan.State, hcan.ErrorCode);

    if (HAL_FDCAN_ConfigGlobalFilter(&hcan, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT, FDCAN_REJECT_REMOTE,
                                     FDCAN_REJECT_REMOTE) != HAL_OK) {
      ESP_LOGE(TAG, "Can't configure global filter");
    } else {
      ESP_LOGI(TAG, "global filter configured");
    }

    FDCAN_FilterTypeDef fconfig = {0};
    fconfig.IdType = FDCAN_STANDARD_ID;
    fconfig.FilterIndex = 0;
    fconfig.FilterType = FDCAN_FILTER_MASK;
    fconfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    fconfig.FilterID1 = 0x0;
    fconfig.FilterID2 = 0x0;

    if (HAL_FDCAN_ConfigFilter(&hcan, &fconfig) == HAL_OK) {
      ESP_LOGI(TAG, "filter configured");
    }

    if (HAL_FDCAN_Start(&hcan) == HAL_OK) {
      ESP_LOGI(TAG, "FDCAN: Started");
    }

    // filter.FilterActivation = CAN_FILTER_ENABLE;
    // filter.FilterScale = CAN_FILTERSCALE_16BIT;
    // filter.FilterMode = CAN_FILTERMODE_IDMASK;
    // filter.FilterIdHigh = 0;
    // filter.FilterIdLow = 0;
    // filter.FilterMaskIdHigh = 0;
    // filter.FilterMaskIdLow = 0;
    // filter.FilterFIFOAssignment = 0;
    // filter.FilterBank = 0;
    // filter.SlaveStartFilterBank = 0;

    // if (HAL_CAN_ConfigFilter(&hcan, &filter) == HAL_OK) {
    //   ESP_LOGI(TAG, "CAN: filter configured");
    // }
    // if (HAL_CAN_Start(&hcan) == HAL_OK) {
    //   ESP_LOGI(TAG, "CAN: Started");
    // } else {
    //   ESP_LOGE(TAG, "CAN: Can't start");
    // }
    return true;
  }
  return false;
}
static uint32_t last_millis = 0;

void STM32FDCan::loop() {
  uint32_t t = millis();
  if (t - last_millis < 1000) {
    return;
  }
  last_millis = t;
  uint32_t fill_rx0 = HAL_FDCAN_GetRxFifoFillLevel(&hcan, FDCAN_RX_FIFO0);
  uint32_t fill_rx1 = HAL_FDCAN_GetRxFifoFillLevel(&hcan, FDCAN_RX_FIFO1);
  FDCAN_ErrorCountersTypeDef cnts = {0};
  FDCAN_ProtocolStatusTypeDef status = {0};
  HAL_FDCAN_GetErrorCounters(&hcan, &cnts);
  HAL_FDCAN_GetProtocolStatus(&hcan, &status);

  ESP_LOGV(TAG,
           "rx fill[0: %d, 1: %d], errors[tx: %d, rx: %d, rx_passive: %d, cnt: %d], activity: %x, err_passive: %d, "
           "warn: %d, bus_off: %d, last_err: %d",
           fill_rx0, fill_rx1, cnts.TxErrorCnt, cnts.RxErrorCnt, cnts.RxErrorPassive, cnts.ErrorLogging,
           status.Activity, status.ErrorPassive, status.Warning, status.BusOff, status.LastErrorCode);
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

  uint32_t free_tx = HAL_FDCAN_GetTxFifoFreeLevel(&hcan);

  if (!free_tx) {
    ESP_LOGE(TAG, "output queues are full");
    return canbus::ERROR_ALLTXBUSY;
  }

  ESP_LOGV(TAG, "free tx buffers: %ld", free_tx);

  if (HAL_FDCAN_AddMessageToTxFifoQ(&hcan, &TxHeader, frame->data) == HAL_OK) {
    ESP_LOGV(TAG, "message sent, state: %d, err: %ld", hcan.State, hcan.ErrorCode);
  } else {
    ESP_LOGE(TAG, "can't send message");
  }
  return canbus::ERROR_OK;
};

canbus::Error STM32FDCan::read_message(struct canbus::CanFrame *frame) {
#if !defined(FDCAN1)
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
#else
#warning TODO - add FDCAN support
#endif
  return canbus::ERROR_NOMSG;
};

}  // namespace stm32_fdcan
}  // namespace esphome
#endif
