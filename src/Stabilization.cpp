#include <avr/wdt.h>
#include "Stabilization.h"

void Stabilization::Init(Reception &_Rx) {
    // ESC
    ESCs.Init();

    // MPU6050: join I2C bus
    Wire.begin();
    Wire.setClock(400000L); // Communication with MPU-6050 at 400KHz

    while (!_Rx.IsReady()) {
        Serial.println(F("Rx not ready, try again, please wait. "));
        ESCs.Idle();
        wdt_reset();
        delay(200);
    }
    // MPU6050, MS5611: initialize MPU6050 and MS5611 devices (IMU and
    // barometer)

    attitude.Init();

    if ((ESCs.MAX_POWER == 1860) && (ESCs.MAX_THROTTLE >= (1860 * 0.8)))
        Serial.println(
                F("!!!!!!!!!!!!!!!!!!!!FLYING MODE "
                  "POWER!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! "));
    else if ((ESCs.MAX_POWER <= 1300))
        Serial.println(F("DEBUG MODE POWER!!! "));
    else
        Serial.println(F("UNEXPECTED POWER "));

    Serial.print(F("MAX_POWER: "));
    Serial.print(ESCs.MAX_POWER);
    Serial.print(F(" MAX_THROTTLE_PERCENT: "));
    Serial.println(ESCs.MAX_THROTTLE_PERCENT);
    // Angle mode PID config
    rollPosPID_Angle.SetGains(anglePosPIDParams);
    pitchPosPID_Angle.SetGains(anglePosPIDParams);
    rollSpeedPID_Angle.SetGains(angleSpeedPIDParams);
    pitchSpeedPID_Angle.SetGains(angleSpeedPIDParams);

    // Adjust Kp from potentiometer on A0
    yawSpeedPIDParams[1] = map(analogRead(0), 0, 1023, 0, 500);
    Serial.print("Yaw kP: ");
    Serial.println(yawSpeedPIDParams[1]);
    yawSpeedPID_Angle.SetGains(yawSpeedPIDParams);

    // Accro mode PID config
    rollSpeedPID_Accro.SetGains(accroSpeedPIDParams);
    pitchSpeedPID_Accro.SetGains(accroSpeedPIDParams);
    yawSpeedPID_Accro.SetGains(yawSpeedPIDParams);
}

void Stabilization::Accro(float _loopTimeSec, Reception &_Rx, const int _throttle) {
    // Get current attitude (roll, pitch, yaw speeds)
    attitude.GetCurrPos(posCurr, speedCurr, _loopTimeSec);

    // Compute new speed commandi for each axis
    rollMotorPwr = rollSpeedPID_Accro.ComputeCorrection(_Rx.GetAileronsSpeed(), speedCurr[0],
                                                        _loopTimeSec);
    pitchMotorPwr = pitchSpeedPID_Accro.ComputeCorrection(_Rx.GetElevatorSpeed(), speedCurr[1],
                                                          _loopTimeSec);
    yawMotorPwr = yawSpeedPID_Accro.ComputeCorrection(_Rx.GetRudder(), speedCurr[2], _loopTimeSec);

    // Apply computed speed command to motors
    SetMotorsPwrXConfig(_throttle);
}

void Stabilization::Angle(float _loopTimeSec, Reception &_Rx, const int _throttle) {
    // Get current attitude (roll, pitch, yaw angles and speeds)
    attitude.GetCurrPos(posCurr, speedCurr, _loopTimeSec);

    // Compute roll position command
    rollPosCmd =
            rollPosPID_Angle.ComputeCorrection(_Rx.GetAileronsAngle(), posCurr[0], _loopTimeSec);

    // Compute roll speed command
    rollMotorPwr = rollSpeedPID_Angle.ComputeCorrection(rollPosCmd, speedCurr[0], _loopTimeSec);

    // Compute pitch position command
    pitchPosCmd =
            pitchPosPID_Angle.ComputeCorrection(_Rx.GetElevatorAngle(), posCurr[1], _loopTimeSec);

    // Compute pitch speed command
    pitchMotorPwr = pitchSpeedPID_Angle.ComputeCorrection(pitchPosCmd, speedCurr[1], _loopTimeSec);

    // Compute yaw speed command
    yawMotorPwr = yawSpeedPID_Angle.ComputeCorrection(_Rx.GetRudder(), speedCurr[2], _loopTimeSec);

    // Apply computed command to motors
    SetMotorsPwrXConfig(_throttle);
}

void Stabilization::PrintAccroModeParameters() {
    Serial.println(F("/********* PID settings *********/"));
    rollSpeedPID_Accro.PrintGains();
    pitchSpeedPID_Accro.PrintGains();
    yawSpeedPID_Accro.PrintGains();
    Serial.print(F("Mixing: "));
    Serial.println(mixing);
}

void Stabilization::PrintAngleModeParameters() {
    Serial.println(F("/********* PID settings *********/"));
    rollPosPID_Angle.PrintGains();
    pitchPosPID_Angle.PrintGains();

    rollSpeedPID_Angle.PrintGains();
    pitchSpeedPID_Angle.PrintGains();
    yawSpeedPID_Angle.PrintGains();
    Serial.println(F("/********* Complementary filter *********/"));
    Serial.print("Coefficient: ");
    Serial.print(attitude.HighPassFilterCoeff);
    Serial.print(" Time constant: ");
    Serial.println(attitude.GetFilterTimeConstant(0.00249));
    Serial.print(F("Mixing: "));
    Serial.println(mixing);
}

void Stabilization::ResetPID(const int _throttle) {
    pitchMotorPwr = rollMotorPwr = yawMotorPwr = 0; // No correction if throttle put to min
    rollPosPID_Angle.Reset();
    pitchPosPID_Angle.Reset();
    rollSpeedPID_Angle.Reset();
    pitchSpeedPID_Angle.Reset();
    yawSpeedPID_Angle.Reset();
    rollSpeedPID_Accro.Reset();
    pitchSpeedPID_Accro.Reset();
    yawSpeedPID_Accro.Reset();

    SetMotorsPwrXConfig(_throttle);
}

//    X configuration:
//  ESC0(CCW)  ESC1
//         \  /
//         /  \
//     ESC3   ESC2(CCW)
//
void Stabilization::SetMotorsPwrXConfig(const int _throttle) {
    ESCs.write(ESC0,
               _throttle - pitchMotorPwr * mixing + rollMotorPwr * mixing - yawMotorPwr * mixing);
    ESCs.write(ESC1,
               _throttle - pitchMotorPwr * mixing - rollMotorPwr * mixing + yawMotorPwr * mixing);
    ESCs.write(ESC2,
               _throttle + pitchMotorPwr * mixing - rollMotorPwr * mixing - yawMotorPwr * mixing);
    ESCs.write(ESC3,
               _throttle + pitchMotorPwr * mixing + rollMotorPwr * mixing + yawMotorPwr * mixing);
}

void Stabilization::Idle() {
    ESCs.Idle();
}
