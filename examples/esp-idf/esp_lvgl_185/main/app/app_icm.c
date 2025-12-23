// #include "string.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/semphr.h"
// #include "driver/spi_master.h"
// #include "driver/gpio.h"
// #include "esp_heap_caps.h"
// #include "esp_log.h"
// #include "app_bmi270.h"
// #include "esp_system.h"
// #include "esp_check.h"
// #include "bsp/esp-bsp.h"
// #include "inv_imu_apex.h"
// #include "app_espnow.h"
// #include "ui.h"
// #include "app_icm.h"

// static const char *TAG = "app_icm";
// QueueHandle_t icm_espnow_send_queue;
// QueueHandle_t icm_espnow_receive_queue;

// uint8_t icm_start = 0;

// static void game_end(ball_t* ball,icm_button_t* buttonmy,icm_button_t* buttonother) 
// {
//     icm_start=0;
//     _ui_flag_modify(ui_ButtonIMUstart, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
//     lv_label_set_text(ui_LabelButtonIMUstart, "AGAIN");
//     lv_task_handler();
//     ball->x = 0;
//     ball->y = 160;
//     ball->z = 100;
//     ball->velocity_x = 0;
//     ball->velocity_y = 0;
//     ball->velocity_z = 2;
//     buttonother->status = 1;
//     buttonother->width = 100;
//     buttonmy->width = 100;
//     lv_obj_set_width(ui_ButtonIMUmy, buttonmy->width);
//     lv_obj_set_width(ui_ButtonIMUother, buttonother->width);
// }

// static void hit_detection(icm_button_t* button, ball_t* ball, uint8_t* turn,icm_button_t* buttonmy,icm_button_t* buttonother) 
// {
//     if (ball->y < -77.5 && button->status == 1) {
//         if (ball->x > button->x-(button->width/2) && ball->x < button->x+button->width/2) {
//             *turn = !*turn;
//             ball->z = 0;
//             ESP_LOGI(TAG, "turn = %d", *turn);
//             ball->velocity_x = (ball->x - button->x)*4;
//             ball->y = -77.5;
//             ball->velocity_y = -ball->velocity_y;
//             // button->width -= 5;
//             // if (button->width<=10) button->width = 10;
//             lv_obj_set_width(ui_ButtonIMUmy, button->width);
//         }
//         else {
//             button->status = 0;
//         }
//     }else if (ball->y <-160){
//         game_end(ball, buttonmy, buttonother);
//     }
// }

// float GRAVITY = 558;
// float TIME_STEP = 0.01;

// void ball_ctrl(ball_t *ball,icm_button_t* buttonmy,icm_button_t* buttonother, uint8_t* turn)
// {
//     if (icm_start) {
//       ball->velocity_y += GRAVITY * TIME_STEP;
//       ball->y -= ball->velocity_y * TIME_STEP;
//       if (*turn ==0 ){
//           hit_detection(buttonmy, ball, turn, buttonmy, buttonother);
//       }else {
//           hit_detection(buttonother, ball, turn, buttonmy, buttonother);
//       }
      
//       ball->z += ball->velocity_z * TIME_STEP;
//       ball->x += ball->velocity_x * TIME_STEP;
//       if (ball->x < -170) {
//         ball->x = -170;
//         ball->velocity_x = -ball->velocity_x;
//       }else if (ball->x > 170){
//         ball->x = 170;
//         ball->velocity_x = -ball->velocity_x;
//       }
//     }
// }

// void bmi270_task(void *pvParameters)
// {
//     bmi270_init();
//     bmi270_value_t gyro_value_cal;
//     icm42670_calibrate_gyro(icm42670_handle, NULL,&gyro_value_cal);
//     icm_espnow_quenedata_t icm_espnow_quenedata,icm_espnow_send_quenedata;
//     bmi270_value_t value[2];
//     complimentary_angle_t complimentary_angle;
//     int log_cnt=0;
//     uint8_t turn =0;
//     uint8_t ball_pos=0;
//     ball_t ball = {
//         .x = 0,
//         .y = 160,
//         .z = 0,
//         .velocity_x = 0,
//         .velocity_y = 0,
//         .velocity_z = 10,
//         .size       = 15,
//         .transparency = 255
//     };
//     icm_button_t ui_ButtonIMUmy_value = {
//             .width = 100,
//             .status = 1
//     };
//     icm_button_t ui_ButtonIMUother_value = {
//             .width = 100,
//             .status = 1
//     };
//     lv_obj_set_width(ui_ButtonIMUother, 50);
//     lv_obj_set_height(ui_ButtonIMUother, 12);
//     lv_obj_set_x(ui_ButtonIMUother, 0);
//     lv_obj_set_y(ui_ButtonIMUother, -41);
//     lv_obj_set_style_bg_opa(ui_ButtonIMUother, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
//     _ui_flag_modify(ui_ButtonIMUstart, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
//     lv_task_handler();
//     while(1){
//         int ret = bmi270_get_gyro_value(&value[0]);
//         ret = bmi270_get_acce_value(&value[1]);
//         ret = icm42670_complimentory_filter(icm42670_handle, &value[1], &value[0], &gyro_value_cal, &complimentary_angle);

//         if (xQueueReceive(icm_espnow_receive_queue, &icm_espnow_quenedata, 0) == pdTRUE) {
//             lv_obj_set_x(ui_ButtonIMUother, icm_espnow_quenedata.line_x/2);
//             lv_obj_set_y(ui_ButtonIMUother, -41);
//         }
//         complimentary_angle_t screen_value,screen_value1;
//         screen_value.roll = 5*complimentary_angle.roll;
//         screen_value.pitch = -5*complimentary_angle.pitch;
//         screen_value1.pitch = screen_value.roll*0.737-screen_value.pitch*0.676;
//         screen_value1.roll = screen_value.pitch*0.737+screen_value.roll*0.676;
//         ui_ButtonIMUmy_value.x = screen_value1.pitch;
//         ui_ButtonIMUmy_value.y = screen_value1.roll;
//         if (ui_ButtonIMUmy_value.x > (180 - ui_ButtonIMUmy_value.width/2)) {
//             ui_ButtonIMUmy_value.x = (180 - ui_ButtonIMUmy_value.width/2);
//         } else if (ui_ButtonIMUmy_value.x < (-180 + ui_ButtonIMUmy_value.width/2)) {
//             ui_ButtonIMUmy_value.x = (-180 + ui_ButtonIMUmy_value.width/2);
//         }
//         lv_obj_set_x(ui_ButtonIMUmy, ui_ButtonIMUmy_value.x);
//         lv_obj_set_y(ui_ButtonIMUmy, 100);//ui_ButtonIMUmy_value.roll);
//         ui_ButtonIMUother_value.x = icm_espnow_quenedata.line_x;
//         ui_ButtonIMUother_value.y = icm_espnow_quenedata.line_y;
        


//         if (icm_espnow_quenedata.status == 0) {     
//             ball_ctrl(&ball,&ui_ButtonIMUmy_value,&ui_ButtonIMUother_value,&turn);   
//             if (turn ==1)  lv_obj_set_style_bg_color(ui_Button1, lv_color_hex(0xFAFD8A), LV_PART_MAIN | LV_STATE_DEFAULT);
//             else   lv_obj_set_style_bg_color(ui_Button1, lv_color_hex(0x73E5EF), LV_PART_MAIN | LV_STATE_DEFAULT);
//             if ((ball.velocity_y > 0 && turn == 0) || (ball.velocity_y < 0 && turn == 1)) {
//               ball.size = 10*(474-(77+ball.y))/474+10;
//               lv_obj_set_y(ui_Button1, -ball.y);
//               ball_pos=0;
//             }else{
//               ball.size = 10*(77+ball.y)/474+10;
//               lv_obj_set_y(ui_Button1, -(160-(237-(ball.y+77))/2));
//               ball_pos=1;
//             }
//             lv_obj_set_x(ui_Button1, ball.x*((ball.size-10)/20+0.5)); 
//             //lv_obj_set_x(ui_Button1, ball.x);
//             // lv_obj_set_y(ui_Button1, -ball.y);
//             lv_obj_set_width(ui_Button1, ball.size);
//             lv_obj_set_height(ui_Button1, ball.size);
//         } else {
//             if ((icm_espnow_quenedata.ball_pos & 0x2)>>1) {
//               if(!lv_obj_has_flag(ui_ButtonIMUstart, LV_OBJ_FLAG_HIDDEN)) {
//                 _ui_flag_modify(ui_ButtonIMUstart, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
//                 lv_task_handler();
//               }
//             }
//             else {
//               if(lv_obj_has_flag(ui_ButtonIMUstart, LV_OBJ_FLAG_HIDDEN)) {
//                 _ui_flag_modify(ui_ButtonIMUstart, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
//                 lv_task_handler();
//               }
//             }
//             turn = icm_espnow_quenedata.turn;
//             if (turn ==0)  lv_obj_set_style_bg_color(ui_Button1, lv_color_hex(0xFAFD8A), LV_PART_MAIN | LV_STATE_DEFAULT);
//             else   lv_obj_set_style_bg_color(ui_Button1, lv_color_hex(0x73E5EF), LV_PART_MAIN | LV_STATE_DEFAULT);
//             if ((icm_espnow_quenedata.ball_pos & 0x1)==1) {
//               ball.size = 10*(474-(77+icm_espnow_quenedata.ball_y))/474+10;
//               lv_obj_set_y(ui_Button1, -icm_espnow_quenedata.ball_y);
//             }else{
//               ball.size = 10*(77+icm_espnow_quenedata.ball_y)/474+10;
//               lv_obj_set_y(ui_Button1, -(160-(237-(icm_espnow_quenedata.ball_y+77))/2));
//             }
//             lv_obj_set_width(ui_Button1, ball.size);
//             lv_obj_set_height(ui_Button1, ball.size);

//             lv_obj_set_x(ui_Button1, icm_espnow_quenedata.ball_x*((ball.size-10)/20+0.5)); 
//         }
//         lv_obj_set_width(ui_ButtonIMUother, ui_ButtonIMUother_value.width/2);

//         icm_espnow_send_quenedata.ball_x = ball.x;
//         icm_espnow_send_quenedata.ball_y = ball.y;
//         icm_espnow_send_quenedata.line_x = ui_ButtonIMUmy_value.x;
//         icm_espnow_send_quenedata.line_y = 100;//ui_ButtonIMUmy_value.roll;
//         icm_espnow_send_quenedata.turn = turn;
//         icm_espnow_send_quenedata.ball_pos = ((icm_start & 0x1)<<1) | (ball_pos & 0x1);
//         xQueueSend(icm_espnow_send_queue, &icm_espnow_send_quenedata, 0);

//         log_cnt++;
//         if (log_cnt > 20) {
//           log_cnt = 0;
//           ESP_LOGI(TAG, "ball.size = %f,ball.x = %f,ball.cal = %f", ball.size, ball.x,ball.x*((ball.size-5)/15));
//         }
//         vTaskDelay(pdMS_TO_TICKS(10));
//     }



// }

// esp_err_t icm42670_init(void)
// {
//     icm_espnow_send_queue = xQueueCreate(6, sizeof(icm_espnow_quenedata_t));
//     icm_espnow_receive_queue = xQueueCreate(6, sizeof(icm_espnow_quenedata_t));
//     app_espnow_init();  
//     xTaskCreatePinnedToCore(bmi270_task, "bmi270_task", 4096, NULL, 4, NULL,1);
//     return ESP_OK;
// }