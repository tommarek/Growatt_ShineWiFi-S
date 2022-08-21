#include <ModbusMaster.h>
#include <ArduinoJson.h>

#include "Growatt124.h"
#include "GrowattTypes.h"
#include "Growatt.h"
#include "Config.h"


ModbusMaster Modbus;

// Constructor
Growatt::Growatt() {
  _eDevice = Undef_stick;
  _PacketCnt = 0;
}

void Growatt::begin(Stream &serial, uint16_t version) {
  /**
   * @brief Set up communication with the inverter
   * @param serial The serial interface
   * @param version The version of the modbus protocol to use
   */
  uint8_t res;

  // choose the right protocol definition based on protocol version
  if (version == 124) {
      _Protocol = init_growatt124();
  } else if (version == 305) {
      //_Protocol = Growatt305;
      _Protocol = init_growatt124();
  } else {
      _Protocol = init_growatt124();
  }

  #if SIMULATE_INVERTER == 1
    _eDevice = SIMULATE_DEVICE;
  #else
    // init communication with the inverter
    Serial.begin(9600);
    Modbus.begin(1, serial);
    res = Modbus.readInputRegisters(0, 1);
    if(res == Modbus.ku8MBSuccess) {
      _eDevice = ShineWiFi_S; // Serial
    } else {
      delay(1000);
      Serial.begin(115200);
      Modbus.begin(1, serial);
      res = Modbus.readInputRegisters(0, 1);
      if(res == Modbus.ku8MBSuccess) {
        _eDevice = ShineWiFi_X; // USB
      }
    }
  #endif
}

eDevice_t Growatt::GetWiFiStickType() {
  /**
   * @brief After initialisation the type of the wifi stick is known
   * @returns eDevice_t type of the wifi stick
   */

  return _eDevice;
}

bool Growatt::_ReadRegisterData(
  uint16_t readCount,
  sGrowattModbusReg_t *registers,
  uint8_t fragmentCount,
  sGrowattReadFragment_t *readFragments
) {
  /**
   * @brief reads the data from the registers (input/holding)
   * @param registers pointer to the register structure
   * @param readFragments pointer to the read fragment structure
   * @returns true if successful, false otherwise
   */
  uint16_t registerAddress;
  uint8_t res;

  // read each fragment separately
  for (int i = 0; i < fragmentCount; i++) {
    res = Modbus.readInputRegisters(
      readFragments[i].StartAddress,
      readFragments[i].FragmentSize
    );
    if(res == Modbus.ku8MBSuccess) {
      for (int j = 0; j < readCount; j++) {
        // make sure the register we try to read is in the fragment
        if (registers[j].address >= readFragments[i].StartAddress) {
          // when we exceed the fragment size, skip to new fragment
          if (registers[j].address >= readFragments[i].StartAddress + readFragments[i].FragmentSize)
            break;
          // let's say the register address is 1013 and read window is 1000-1050
          // that means the response in the buffer is on position 1013 - 1000 = 13
          registerAddress = registers[j].address - readFragments[i].StartAddress;
          if (registers[j].size == SIZE_16BIT) {
            registers[j].value = Modbus.getResponseBuffer(registerAddress);
          } else {
            registers[j].value = Modbus.getResponseBuffer(registerAddress) << 16 + Modbus.getResponseBuffer(registerAddress + 1);
          }
        }
      }
    } else {
      return false;
    }
  }
  _GotData = true;
  return true;
}

bool Growatt::ReadData() {
  /**
   * @brief Reads the data from the inverter and updates the internal data structures
   * @returns true if data was read successfully, false otherwise
   */
  bool input_res = _ReadRegisterData(
    _Protocol.HoldingRegisterCount,
    _Protocol.HoldingRegisters,
    _Protocol.HoldingFragmentCount,
    _Protocol.HoldingReadFragments
    );
  bool holding_res = _ReadRegisterData(
    _Protocol.InputFragmentCount,
    _Protocol.InputRegisters,
    _Protocol.InputFragmentCount,
    _Protocol.InputReadFragments
  );
  _PacketCnt = _PacketCnt + 1;
  return input_res;// && holding_res;
}

sGrowattModbusReg_t Growatt::GetInputRegister(SupportedModbusInputRegisters_t reg) {
  /**
   * @brief get the internal representation of the input register
   * @param reg the register to get
   * @returns the register value
   */
    if (_GotData == false) {
      ReadData();
    }
    return _Protocol.InputRegisters[reg];
}

sGrowattModbusReg_t Growatt::GetHoldingRegister(SupportedModbusHoldingRegisters_t reg) {
  /**
   * @brief get the internal representation of the holding register
   * @param reg the register to get
   * @returns the register value
   */
    if (_GotData == false) {
      ReadData();
    }
    return _Protocol.HoldingRegisters[reg];
}

bool Growatt::ReadHoldingReg(uint16_t adr, uint16_t* result) {
  /**
   * @brief read 16b holding register
   * @param adr address of the register
   * @param result pointer to the result
   * @returns true if successful
   */
    uint8_t res = Modbus.readHoldingRegisters(adr, 1);
    if (res == Modbus.ku8MBSuccess) {
        *result = Modbus.getResponseBuffer(0);
        return true;
    }
    return false;
}

bool Growatt::ReadHoldingReg(uint16_t adr, uint32_t* result) {
  /**
   * @brief read 32b holding register
   * @param adr address of the register
   * @param result pointer to the result
   * @returns true if successful
   */
    uint8_t res = Modbus.readHoldingRegisters(adr, 2);
    if (res == Modbus.ku8MBSuccess) {
        *result = Modbus.getResponseBuffer(0);
        return true;
    }
    return false;
}

bool Growatt::WriteHoldingReg(uint16_t adr, uint16_t value) {
  /**
   * @brief write 16b holding register
   * @param adr address of the register
   * @param value value to write to the register
   * @returns true if successful
   */
    uint8_t res = Modbus.writeSingleRegister(adr, value);
    if (res == Modbus.ku8MBSuccess) {
        return true;
    }
    return false;
}


bool Growatt::ReadInputReg(uint16_t adr, uint16_t* result) {
  /**
   * @brief read 16b input register
   * @param adr address of the register
   * @param result pointer to the result
   * @returns true if successful
   */
    uint8_t res = Modbus.readInputRegisters(adr, 1);
    if (res == Modbus.ku8MBSuccess) {
        *result = Modbus.getResponseBuffer(0);
        return true;
    }
    return false;
}

bool Growatt::ReadInputReg(uint16_t adr, uint32_t* result) {
  /**
   * @brief read 32b input register
   * @param adr address of the register
   * @param result pointer to the result
   * @returns true if successful
   */
    uint8_t res = Modbus.readInputRegisters(adr, 2);
    if (res == Modbus.ku8MBSuccess) {
        *result = Modbus.getResponseBuffer(0);
        return true;
    }
    return false;
}

eGrowattStatus_t Growatt::GetStatus() {
  /**
   * @brief Returns the status of the inverter
   * @returns eGrowattStatus_t status of the inverter
   */
  return (eGrowattStatus_t)Growatt::GetInputRegister(I_STATUS).value;
}

float Growatt::GetAcPower() {
  /**
   * @brief Returns the status of the inverter
   * @returns eGrowattStatus_t status of the inverter
   */
  return 1.0; //TODO!
}

void Growatt::CreateJson(char *Buffer, const char *MacAddress) {
  StaticJsonDocument<2048> doc;
  JsonObject input = doc.createNestedObject("input");
  JsonObject holding = doc.createNestedObject("holding");

  input[_Protocol.InputRegisters[0].name] = _Protocol.InputRegisters[0].value * _Protocol.InputRegisters[0].multiplier;

#if SIMULATE_INVERTER != 1
  // for (int i = 0; i < _Protocol.InputRegisterCount; i++) {
  //   input[_Protocol.InputRegisters[i].name] = _Protocol.InputRegisters[i].value * _Protocol.InputRegisters[i].multiplier;
  // }
  // for (int i = 0; i < _Protocol.HoldingRegisterCount; i++) {
  //   holding[_Protocol.HoldingRegisters[i].name] = _Protocol.HoldingRegisters[i].value * _Protocol.HoldingRegisters[i].multiplier;
  // }
#else
  #warning simulating the inverter
  input['Status'] = 1;
  input['DcPower'] = 230;
  input['DcVoltage'] = 70.5;
  input['DcInputCurrent'] = 8.5;
  input['AcFreq'] = 50.00;
  input['AcVoltage'] = 230.0;
  input['AcPower'] = 0.00;
  input['EnergyToday'] = 0.3;
  input['EnergyTotal'] = 49.1;
  input['OperatingTime'] = 123456;
  input['Temperature'] = 21.12;
  input['AccumulatedEnergy'] = 320;
  input['EnergyToday'] = 0.3;
  input['EnergyToday'] = 0.3;
#endif // SIMULATE_INVERTER
  doc['Mac'] = MacAddress;
  doc['Cnt'] = _PacketCnt;

  serializeJson(doc, Buffer, 4096);
}
