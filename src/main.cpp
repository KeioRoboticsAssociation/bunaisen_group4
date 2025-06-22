#include <UnbufferedSerial.h>
#include <mbed.h>
#include <stdio.h>

#define BAUD 115200
static UnbufferedSerial esp(PB_10, PC_5);
DigitalOut led(LED1);


// モータードライバのPWM出力ピンを指定 (ご自身の環境に合わせて変更してください)
PwmOut motorL(PA_15); 
PwmOut motorR(PB_7);
DigitalOut motorL_dir(PA_13); // 左モーター方向ピン1
DigitalOut motorR_dir(PA_4); // 右モーター方向ピン1

// --- ここから追加 ---
// 常時回転させるモーターのピンを指定
PwmOut continuousMotor(PA_1); // ご自身の環境に合わせてピンを変更してください
DigitalOut continuousMotor_dir(PA_0); // 常時回転モーターの方向ピン
// --- ここまで追加 ---


char buf[2];
char str[64];
asm(".global _printf_float"); // printfでfloatを使えるようにする

struct Gamepad
{
    // データを扱う構造体
    bool button[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int dpad = 0;
    float rstick[2] = {0.0, 0.0};
    float lstick[2] = {0.0, 0.0};
} joy;

void read_esp()
{
    // 割り込み関数
    static int i = 0;
    esp.read(buf, 2);
    // 1文字ずつ読む
    if (i >= 64)
    {
        i = 0;
        memset(str, '\0', sizeof(str));
    }
    if (buf[0] == '\n')
    { // 改行コードが来たら
        i = 0;
        // printf("recv: %s\n", str);
        // 受信したデータを表示
        int data[6];
        sscanf(str, "%d %d %d %d %d %d\n", &data[0], &data[1], &data[2], &data[3], &data[4], &data[5]);
        joy.rstick[0] = data[0] / 512.0 ;
        joy.rstick[1] = data[1] / 512.0;
        joy.lstick[0] = data[2] / 512.0 ;
        joy.lstick[1] = data[3] / 512.0 ;
        joy.dpad = data[4];
        for (int j = 0; j < 12; j++)
        {
            joy.button[j] = (data[5] & (1 << j)) >> j;
        }
        memset(str, '\0', sizeof(str));
    }
    else
    { // 改行コードが来るまで
        str[i] = buf[0];
        i++;
    }
}

void init()
{
    esp.baud(BAUD);                                 // 通信速度の指定
    esp.attach(&read_esp, UnbufferedSerial::RxIrq); // 割り込み関数の割当
}

int main(void)
{
    init();
    float speedfactor = 0.5f; // スピードファクターの初期値
    int dpad = 0; // 十字キーの状態を保持する変数
    int button = 0; // ボタンの状態を保持する変数
    int continuousMotor_ = 1; // 常時回転モーターの速度を保持する変数
    // --- ここから追加 ---
    // 常時回転モーターのPWM周期とデューティ比（速度）を設定
    // デューティ比は0.0 (停止) から 1.0 (最大速度) の間で指定
    continuousMotor.period_us(100);  // PWM周期を20msに設定
    continuousMotor.write(0.2f);    // 50%の速度で回転
    continuousMotor_dir.write(continuousMotor_); // 常時回転モーターの方向を初期化 (1: 正転, 0: 逆転)
    // --- ここまで追加 ---

    motorL.period_us(100); // PWM周期を20msに設定
    motorR.period_us(100); // PWM周期を20msに設定
    motorL_dir.write(0); // 左モーターの方向を初期化
    motorR_dir.write(0); // 右モーターの方向を初期
    motorL.write(0.0f); // 左モーターのデューティ比を初期化
    motorR.write(0.0f); // 右モーターのデューティ比を初期化

    while (1) //コメントはdualshock4の場合
    {
        if(joy.dpad == 1){
            dpad = 1; // 上
        }
        else if(joy.dpad == 2){
            dpad = -1; // 下
        }
        else {
            dpad = 0;
        }

        if(joy.button[4]){
            button = 1; // L1ボタンが押された場合
        }
        else if(joy.button[5]){
            button = -1; // R1ボタンが押された場合
        }
        else {
            button = 0; // ボタンが押されていない場合
        }
        // 左スティックのY軸で前後の速度、X軸で旋回速度を決定
        float forward = -dpad; // Y軸の値を取得 (-1.0 ~ 1.0)
        float turn = button;    // X軸の値を取得 (-1.0 ~ 1.0)

        // 各モーターの速度を計算（ミキシング）
        float left_speed = (forward + turn)/2;
        float right_speed = (forward - turn)/2;
        float deadzone = 0.1f; // デッドゾーンの設定
       
        
        // PwmOutは0.0から1.0の値を取るため、速度を正規化
        // 前進・後進を制御するために、モータードライバの仕様に合わせて
        // 別のDigitalOutピンで方向を制御する必要があるかもしれません。
        // ここでは単純化のため、絶対値をデューティ比とします。
        if(left_speed < -deadzone){
          motorL_dir.write(0);
          motorL.write(abs(left_speed * speedfactor)); // speedfactorを掛けて速度を調整
        }
        else if(left_speed > deadzone){
          motorL_dir.write(1);
          motorL.write(abs(left_speed * speedfactor)); // speedfactorを掛けて速度を調整
        } 
        else {
          motorL.write(0.0f); // デッドゾーン内では停止
        }
        if(right_speed < -deadzone){
          motorR_dir.write(1);
          motorR.write(abs(right_speed * speedfactor)); // speedfactorを掛けて速度を調整
        }
        else if(right_speed > deadzone){
          motorR_dir.write(0);
          motorR.write(abs(right_speed * speedfactor)); // speedfactorを掛けて速度を調整    
        } 
        else {
          motorR.write(0.0f); // デッドゾーン内では停止
        }

        if(joy.button[2]){
            if(continuousMotor_ == 1){
                continuousMotor_ = 0; // 逆転
            } else {
                continuousMotor_ = 1; // 正転
            }
            continuousMotor_dir.write(continuousMotor_); // 常時回転モーターを正転
        }
        
       
      
        printf("%5.2f %5.2f %5.2f %5.2f %d %d %d %d %d %d %d %d %d\n",
               joy.rstick[0],  // 左スティックX(左-1.0~1.0右)
               joy.rstick[1],  // 左スティックY(上-1.0~1.0下)
               joy.lstick[0],  // 右スティックX(左-1.0~1.0右)
               joy.lstick[1],  // 右スティックY(上-1.0~1.0下)
               joy.dpad,       // 十字キー(1:上, 2:下, 4:右, 8:左)
               joy.button[0],  // ×
               joy.button[1],  // ○
               joy.button[2],  // ⬜︎
               joy.button[3],  // △
               joy.button[4],  // L1
               joy.button[5],  // R1
               joy.button[6],  // L2
               joy.button[7]); // R2
        
        ThisThread::sleep_for(10ms); // CPU負荷を軽減
    }
    return 0;
}