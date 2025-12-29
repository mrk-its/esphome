#ifdef USE_ZEPHYR

#include "esphome.h"
#include "zephyr_can.h"
namespace esphome {
namespace zephyr_can {

void ZephyrCan::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ZephyrCan");
  if (!this->setup_internal()) {
    ESP_LOGE(TAG, "setup error!");
    this->mark_failed();
  }
  ESP_LOGCONFIG(TAG, "done");
}

void rx_callback(const struct device *dev, struct can_frame *frame, void *user_data) { ESP_LOGI(TAG, "received!"); }

bool ZephyrCan::setup_internal() {
  if (!device_is_ready(this->can_dev_)) {
    return false;
  }
  if (can_stop(this->can_dev_) == 0) {
    ESP_LOGI(TAG, "stopped");
  } else {
    ESP_LOGE(TAG, "cant stop");
  }

  // TODO - do not hardcode bitrate
  // if (can_set_bitrate(this->can_dev_, 125000) != 0) {
  //   ESP_LOGE(TAG, "cant set bitrate");
  //   return false;
  // }
  struct can_timing timing = {0};
  if (can_calc_timing(this->can_dev_, &timing, 125000, 875) != 0) {
    ESP_LOGE(TAG, "cant calc timing");
    return false;
  }
  if (can_set_timing(this->can_dev_, &timing) != 0) {
    ESP_LOGE(TAG, "cant set timing");
    return false;
  }

  if (can_set_mode(this->can_dev_, CAN_MODE_NORMAL) != 0) {
    ESP_LOGE(TAG, "cant set mode");
    return false;
  }
  ESP_LOGI(TAG, "mode set");

  if (can_start(this->can_dev_) != 0) {
    ESP_LOGE(TAG, "can't start");
    return false;
  }

  const struct can_filter my_filter = {.id = 0, .mask = 0, .flags = 0};
  int filter_id;
  filter_id = can_add_rx_filter_msgq(this->can_dev_, this->rx_queue_, &my_filter);
  if (filter_id < 0) {
    ESP_LOGE(TAG, "unable to add rx msgq [%d]", filter_id);
  }

  return true;
}

static can_state old_state = CAN_STATE_ERROR_ACTIVE;
static int old_rx_err_cnt = -1;
static int old_tx_err_cnt = -1;

void ZephyrCan::loop() {
  canbus::Canbus::loop();
  // struct can_frame rx_frame;
  // if (k_msgq_get(this->rx_queue_, &rx_frame, K_MSEC(20)) == 0) {
  //   ESP_LOGI(TAG, "received!");
  // }
  struct can_bus_err_cnt err_cnt;
  enum can_state state;
  if (can_get_state(this->can_dev_, &state, &err_cnt) == 0) {
    if (state != old_state || old_rx_err_cnt != err_cnt.rx_err_cnt || old_tx_err_cnt != err_cnt.tx_err_cnt) {
      ESP_LOGI(TAG, "can_state: %d, rx_err: %d, tx_err: %d", state, err_cnt.rx_err_cnt, err_cnt.tx_err_cnt);
      old_tx_err_cnt = err_cnt.tx_err_cnt;
      old_rx_err_cnt = err_cnt.rx_err_cnt;
      old_state = state;
    }
  } else {
    ESP_LOGE(TAG, "can't get can state");
  }

  // rx_frame.flags = 0;
  // rx_frame.id = 1;
  // rx_frame.dlc = 1;

  // if (can_send(this->can_dev_, &rx_frame, K_MSEC(100), NULL, NULL) == 0) {
  //   ESP_LOGI(TAG, "sent");
  // } else {
  //   ESP_LOGE(TAG, "cant send");
  // }
}

canbus::Error ZephyrCan::send_message(struct canbus::CanFrame *frame) {
  struct can_frame tx_frame = {.id = frame->can_id, .dlc = frame->can_data_length_code, .flags = 0};
  if (frame->can_data_length_code) {
    memcpy(tx_frame.data, frame->data, frame->can_data_length_code);
  }
  int ret = can_send(this->can_dev_, &tx_frame, K_MSEC(20), NULL, NULL);  // K_MSEC(10)  // K_FOREVER
  if (ret < 0) {
    return canbus::ERROR_ALLTXBUSY;
  }
  return canbus::ERROR_OK;
};

canbus::Error ZephyrCan::read_message(struct canbus::CanFrame *frame) {
  struct can_frame rx_frame = {0};
  if (k_msgq_get(this->rx_queue_, &rx_frame, K_MSEC(20)) == 0) {
    frame->can_id = rx_frame.id;
    frame->can_data_length_code = rx_frame.dlc;
    frame->use_extended_id = false;  // TODO
    if (rx_frame.dlc) {
      memcpy(frame->data, rx_frame.data, rx_frame.dlc);
    }
    return canbus::ERROR_OK;
  }
  return canbus::ERROR_NOMSG;
};

}  // namespace zephyr_can
}  // namespace esphome

#endif
