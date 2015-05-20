/***************************************************************************************
Emulates data coming from all sensors on AoT device and assembles them into a packet.
Operates as I2C slave that responds to master's data requests.

Arduino Mega2560 connections:
  GND ---> GND
  SDA (Pin 20) ---> Master's SDA
  SCL (Pin 21) ---> Master's SCL

Author: Daniel Schrader (dschrader@anl.gov)

Last updated: 20 May 2015
***************************************************************************************/



/** Includes **************************************************************************/
#include <Wire.h>
/**************************************************************************************/



/** Constants *************************************************************************/
const byte I2C_SLAVE_ADDRESS = 0x03;

const int DELAY_MS = 30000;

const byte START_BYTE = 0xAA;
const byte END_BYTE = 0x55;
const byte HEADER_RESERVED = 0x00; // Upper nibble
const byte HEADER_VERSION = 0x00; // Lower nibble

const int LENGTH_DATA = 194;
const int LENGTH_WHOLE = LENGTH_DATA + 5;
const int LENGTH_FORMAT1 = 2;
const int LENGTH_FORMAT2 = 2;
const int LENGTH_FORMAT3 = 6;
const int LENGTH_FORMAT4 = 2;
const int LENGTH_FORMAT5 = 2;
const int LENGTH_FORMAT6 = 3;
const int LENGTH_HEALTH = 4;

const int NUM_SENSORS = 32; // Includes MAC addresses

const byte ID_MAC = 0x00;
const byte ID_TMP112 = 0x01;
const byte ID_HTU21D = 0x02;
const byte ID_GP2Y1010AU0F = 0x03;
const byte ID_BMP180 = 0x04;
const byte ID_PR103J2 = 0x05;
const byte ID_TSL250RD_1 = 0x06;
const byte ID_MMA8452Q = 0x07;
const byte ID_SPV1840LR5HB_1 = 0x08;
const byte ID_TSYS01 = 0x09;
const byte ID_HMC5883L = 0x0A;
const byte ID_HIH6130 = 0x0B;
const byte ID_APDS9006020 = 0x0C;
const byte ID_TSL260RD = 0x0D;
const byte ID_TSL250RD_2 = 0x0E;
const byte ID_MLX75305 = 0x0F;
const byte ID_ML8511 = 0x10;
const byte ID_D6T = 0x11;
const byte ID_MLX90614 = 0x12;
const byte ID_TMP421 = 0x13;
const byte ID_SPV1840LR5HB_2 = 0x14;
const byte ID_TOTAL_REDUCING_GASES = 0x15;
const byte ID_ETHANOL = 0x16;
const byte ID_NITROGEN_DIOXIDE = 0x17;
const byte ID_OZONE = 0x18;
const byte ID_HYDROGEN_SULPHIDE = 0x19;
const byte ID_TOTAL_OXIDIZING_GASES = 0x1A;
const byte ID_CARBON_MONOXIDE = 0x1B;
const byte ID_SULFUR_DIOXIDE = 0x1C;
const byte ID_SENSIRION = 0x1D;
const byte ID_BOSH = 0x1E;
const byte ID_INTEL_MAC = 0x1F;
const byte ID_HEALTH = 0xFF;
/**************************************************************************************/



/** Global vars ***********************************************************************/
// Main board
byte MAC_ID[LENGTH_FORMAT3 + 2]; // MAC address
byte TMP112[LENGTH_FORMAT1 + 2]; // ambient temp
byte HTU21D[(LENGTH_FORMAT1 * 2) + 2]; // ambient RH & temp
byte GP2Y1010AU0F[LENGTH_FORMAT2 + 2]; // dust density
byte BMP180[LENGTH_FORMAT1 + LENGTH_FORMAT6 + 2]; // atmospheric pressure
byte PR103J2[LENGTH_FORMAT2 + 2]; // light
byte TSL250RD_1[LENGTH_FORMAT2 + 2]; // ambient light (400-950nm)
byte MMA8452Q[(LENGTH_FORMAT1 * 4) + 2]; // 3-axis accel for traffic flow
byte SPV1840LR5HB_1[LENGTH_FORMAT2 + 2]; // sound pressure
byte TSYS01[LENGTH_FORMAT2 + 2]; // ambient temp

// Aux board
byte HMC5883L[(LENGTH_FORMAT4 * 3) + 2]; // magnetic field strength for traffic flow
byte HIH6130[(LENGTH_FORMAT1 * 2) + 2]; // temp and RH inside transparent box
byte APDS9006020[LENGTH_FORMAT2 + 2]; // ambient light inside cavity
byte TSL260RD[LENGTH_FORMAT2 + 2]; // solar near IR
byte TSL250RD_2[LENGTH_FORMAT2 + 2]; // solar visible light
byte MLX75305[LENGTH_FORMAT2 + 2]; // solar visible light
byte ML8511[LENGTH_FORMAT2 + 2]; // solar UV
byte D6T[(LENGTH_FORMAT1 * 17) + 2]; // temp of surrounding objects
byte MLX90614[LENGTH_FORMAT1 + 2]; // temp of pavement
byte TMP421[LENGTH_FORMAT1 + 2]; // temp inside transparent box
byte SPV1840LR5HB_2[LENGTH_FORMAT2 + 2]; // sound pressure

// Intel board
byte total_reducing_gases[LENGTH_FORMAT2 + 2]; // ambient concentration
byte ethanol[LENGTH_FORMAT2 + 2]; // ambient concentration
byte nitrogen_dioxide[LENGTH_FORMAT2 + 2]; // ambient concentration
byte ozone[LENGTH_FORMAT2 + 2]; // ambient concentration
byte hydrogen_sulphide[LENGTH_FORMAT2 + 2]; // ambient concentration
byte total_oxidizing_gases[LENGTH_FORMAT2 + 2]; // ambient concentration
byte carbon_monoxide[LENGTH_FORMAT2 + 2]; // ambient concentration
byte sulfur_dioxide[LENGTH_FORMAT2 + 2]; // ambient concentration
byte sensirion[(LENGTH_FORMAT1 * 2) + 2]; // ambient temp and RH
byte bosh[LENGTH_FORMAT6 + 2]; // atmospheric pressure
byte intel_MAC_ID[LENGTH_FORMAT3 + 2]; // MAC address of Intel board

// Whole packet
byte packet_whole[LENGTH_WHOLE];
// Data sub-packet
byte packet_data[LENGTH_DATA];
// Sub-packets for each format
byte packet_format1[LENGTH_FORMAT1];
byte packet_format2[LENGTH_FORMAT2];
byte packet_format3[LENGTH_FORMAT3];
byte packet_format4[LENGTH_FORMAT4];
byte packet_format5[LENGTH_FORMAT5];
byte packet_format6[LENGTH_FORMAT6];
// Sub-packet for hardware health status
byte packet_health[LENGTH_HEALTH + 2];
byte valid_array[NUM_SENSORS];

// These lengths are calculated at packet assembly
byte length_whole_actual;
byte length_data_actual;

// CRC-8
byte crc = 0x00;
/**************************************************************************************/



/** Arduino: setup ********************************************************************/
void setup() {
  // Join I2C bus as slave
  Wire.begin(I2C_SLAVE_ADDRESS);
  
  // Register interrupt
  Wire.onRequest(requestEvent);
}
/**************************************************************************************/



/** Arduino: loop *********************************************************************/
void loop() {   
  // Generate fake sensor data
  generate_data();
  
  // Put whole packet together
  assemble_packet_whole();
  
  // Simulate sensor data accumulation period
  delay(DELAY_MS);
}
/**************************************************************************************/



/** Generate fake sensor data *********************************************************/
void generate_data()
{
  byte valid;
  byte integer1, integer2, integer3;
  int fractional;
  byte neg;
  int value_i;
  float value_f;
  int valid_index = 0;
  
  
  
  // MAC address (format 3)
  valid = 1; // always valid
  valid_array[valid_index++] = valid; // Store validity in temp array
  MAC_ID[0] = ID_MAC; // ID
  MAC_ID[1] = (valid << 7) | LENGTH_FORMAT3;
  MAC_ID[2] = 0x01;
  MAC_ID[3] = 0x02;
  MAC_ID[4] = 0x03;
  MAC_ID[5] = 0x04;
  MAC_ID[6] = 0x05;
  MAC_ID[7] = 0x06;
  
  
  
  // Temp (format 1)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  TMP112[0] = ID_TMP112;
  TMP112[1] = (valid << 7) | LENGTH_FORMAT1;
  TMP112[2] = packet_format1[0];
  TMP112[3] = packet_format1[1];
  
  
  
  // Temp (format 1)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  HTU21D[0] = ID_HTU21D;
  HTU21D[1] = (valid << 7) | (LENGTH_FORMAT1 * 2);
  HTU21D[2] = packet_format1[0];
  HTU21D[3] = packet_format1[1];
  
  // RH (format 1)
  integer1 = random(0, 101); // percentage
  fractional = random(0, 100);
  value_f = integer1 + ((float)fractional / 100);
  format1(value_f);  // Put it into format 1
  HTU21D[4] = packet_format1[0];
  HTU21D[5] = packet_format1[1];
  
  
  
  // Dust density (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  GP2Y1010AU0F[0] = ID_GP2Y1010AU0F;
  GP2Y1010AU0F[1] = (valid << 7) | LENGTH_FORMAT2;
  GP2Y1010AU0F[2] = packet_format2[0];
  GP2Y1010AU0F[3] = packet_format2[1];
  
  
  
  // Temp (format 1)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  BMP180[0] = ID_BMP180;
  BMP180[1] = (valid << 7) | (LENGTH_FORMAT1 + LENGTH_FORMAT6);
  BMP180[2] = packet_format1[0];
  BMP180[3] = packet_format1[1];
  
  // Atmospheric pressure (format 6)
  integer1 = random(0, 64);
  integer2 = random(0, 256);
  integer3 = random(0, 256);
  value_i = (integer1 << 16) | (integer2 << 8) | integer3;
  neg = random(0, 2);
  if (neg)
    value_i *= -1;
  format6(value_i);
  BMP180[4] = packet_format6[0];
  BMP180[5] = packet_format6[1];
  BMP180[6] = packet_format6[2];
  
  
  
  // Light (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  PR103J2[0] = ID_PR103J2;
  PR103J2[1] = (valid << 7) | LENGTH_FORMAT2;
  PR103J2[2] = packet_format2[0];
  PR103J2[3] = packet_format2[1];
  
  
  
  // Light (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  TSL250RD_1[0] = ID_TSL250RD_1;
  TSL250RD_1[1] = (valid << 7) | LENGTH_FORMAT2;
  TSL250RD_1[2] = packet_format2[0];
  TSL250RD_1[3] = packet_format2[1];
  
  
  
  // Accel_x (format 1)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  MMA8452Q[0] = ID_MMA8452Q;
  MMA8452Q[1] = (valid << 7) | (LENGTH_FORMAT1 * 4);
  MMA8452Q[2] = packet_format1[0];
  MMA8452Q[3] = packet_format1[1];
  
  // Accel_y (format 1)
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  MMA8452Q[4] = packet_format1[0];
  MMA8452Q[5] = packet_format1[1];
  
  // Accel_x (format 1)
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  MMA8452Q[6] = packet_format1[0];
  MMA8452Q[7] = packet_format1[1];
  
  // Accel RMS (format 1)
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  MMA8452Q[8] = packet_format1[0];
  MMA8452Q[9] = packet_format1[1];



  // Sound pressure (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  SPV1840LR5HB_1[0] = ID_SPV1840LR5HB_1;
  SPV1840LR5HB_1[1] = (valid << 7) | LENGTH_FORMAT2;
  SPV1840LR5HB_1[2] = packet_format2[0];
  SPV1840LR5HB_1[3] = packet_format2[1];
  
  
  
  // Temp (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  TSYS01[0] = ID_TSYS01;
  TSYS01[1] = (valid << 7) | LENGTH_FORMAT2;
  TSYS01[2] = packet_format2[0];
  TSYS01[3] = packet_format2[1];
  
  
  
  // Magnetic field strength x (format 4)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 8);
  fractional = random(0, 1000);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 1000);
  if (neg)
    value_f *= -1;
  format4(value_f);  // Put it into format 4
  HMC5883L[0] = ID_HMC5883L;
  HMC5883L[1] = (valid << 7) | (LENGTH_FORMAT4 * 3);
  HMC5883L[2] = packet_format4[0];
  HMC5883L[3] = packet_format4[1];
  
  // Magnetic field strength y (format 4)
  integer1 = random(0, 8);
  fractional = random(0, 1000);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 1000);
  if (neg)
    value_f *= -1;
  format4(value_f);  // Put it into format 4
  HMC5883L[4] = packet_format4[0];
  HMC5883L[5] = packet_format4[1];
  
  // Magnetic field strength z (format 4)
  integer1 = random(0, 8);
  fractional = random(0, 1000);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 1000);
  if (neg)
    value_f *= -1;
  format4(value_f);  // Put it into format 4
  HMC5883L[6] = packet_format4[0];
  HMC5883L[7] = packet_format4[1];
  
  
  
  // Temp inside transparent box (format 1)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  HIH6130[0] = ID_HIH6130;
  HIH6130[1] = (valid << 7) | (LENGTH_FORMAT1 * 2);
  HIH6130[2] = packet_format1[0];
  HIH6130[3] = packet_format1[1];
  
  // RH inside transparent box (format 1)
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  HIH6130[4] = packet_format1[0];
  HIH6130[5] = packet_format1[1];
  
  
  
  // Ambient light inside the cavity (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  APDS9006020[0] = ID_APDS9006020;
  APDS9006020[1] = (valid << 7) | LENGTH_FORMAT2;
  APDS9006020[2] = packet_format2[0];
  APDS9006020[3] = packet_format2[1];
  
  
  
  // Solar near IR (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  TSL260RD[0] = ID_TSL260RD;
  TSL260RD[1] = (valid << 7) | LENGTH_FORMAT2;
  TSL260RD[2] = packet_format2[0];
  TSL260RD[3] = packet_format2[1];
  
  
  
  // Solar visible light (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  TSL250RD_2[0] = ID_TSL250RD_2;
  TSL250RD_2[1] = (valid << 7) | LENGTH_FORMAT2;
  TSL250RD_2[2] = packet_format2[0];
  TSL250RD_2[3] = packet_format2[1];
  
  
  
  // Solar visible light (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  MLX75305[0] = ID_MLX75305;
  MLX75305[1] = (valid << 7) | LENGTH_FORMAT2;
  MLX75305[2] = packet_format2[0];
  MLX75305[3] = packet_format2[1];
  
  
  
  // Solar UV (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  ML8511[0] = ID_ML8511;
  ML8511[1] = (valid << 7) | LENGTH_FORMAT2;
  ML8511[2] = packet_format2[0];
  ML8511[3] = packet_format2[1];
  
  
  
  // Temp of surrounding objects 1 (format 1)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  D6T[0] = ID_D6T;
  D6T[1] = (valid << 7) | (LENGTH_FORMAT1 * 17);
  D6T[2] = packet_format1[0];
  D6T[3] = packet_format1[1];
  
  // Temp of surrounding objects 2-17 (format 1)
  for (int i = 4; i < ((LENGTH_FORMAT1 * 17) + 4); i += 2)
  {
    integer1 = random(0, 128);
    fractional = random(0, 100);
    neg = random(0, 2);
    value_f = integer1 + ((float)fractional / 100);
    if (neg)
      value_f *= -1;
    format1(value_f);  // Put it into format 1
    D6T[i] = packet_format1[0];
    D6T[i + 1] = packet_format1[1];
  }
  
  
  
  // Temp of pavement (format 1)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  MLX90614[0] = ID_MLX90614;
  MLX90614[1] = (valid << 7) | LENGTH_FORMAT1;
  MLX90614[2] = packet_format1[0];
  MLX90614[3] = packet_format1[1];
  
  
  
  // Temp inside transparent box (format 1)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  TMP421[0] = ID_TMP421;
  TMP421[1] = (valid << 7) | LENGTH_FORMAT1;
  TMP421[2] = packet_format1[0];
  TMP421[3] = packet_format1[1];
  
  
  
  // Sound pressure (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  SPV1840LR5HB_2[0] = ID_SPV1840LR5HB_2;
  SPV1840LR5HB_2[1] = (valid << 7) | LENGTH_FORMAT2;
  SPV1840LR5HB_2[2] = packet_format2[0];
  SPV1840LR5HB_2[3] = packet_format2[1];
  
  
  
  // Ambient concentration of total reducing gases (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  total_reducing_gases[0] = ID_TOTAL_REDUCING_GASES;
  total_reducing_gases[1] = (valid << 7) | LENGTH_FORMAT2;
  total_reducing_gases[2] = packet_format2[0];
  total_reducing_gases[3] = packet_format2[1];
  
  
  
  // Ambient concentration of ethanol (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  ethanol[0] = ID_ETHANOL;
  ethanol[1] = (valid << 7) | LENGTH_FORMAT2;
  ethanol[2] = packet_format2[0];
  ethanol[3] = packet_format2[1];
  
  
  
  // Ambient concentration of nitrogen dioxide (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  nitrogen_dioxide[0] = ID_NITROGEN_DIOXIDE;
  nitrogen_dioxide[1] = (valid << 7) | LENGTH_FORMAT2;
  nitrogen_dioxide[2] = packet_format2[0];
  nitrogen_dioxide[3] = packet_format2[1];
  
  
  
  // Ambient concentration of ozone (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  ozone[0] = ID_OZONE;
  ozone[1] = (valid << 7) | LENGTH_FORMAT2;
  ozone[2] = packet_format2[0];
  ozone[3] = packet_format2[1];
  
  
  
  // Ambient concentration of hydrogen sulphide (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  hydrogen_sulphide[0] = ID_HYDROGEN_SULPHIDE;
  hydrogen_sulphide[1] = (valid << 7) | LENGTH_FORMAT2;
  hydrogen_sulphide[2] = packet_format2[0];
  hydrogen_sulphide[3] = packet_format2[1];
  
  
  
  // Ambient concentration of total oxidizing gases (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  total_oxidizing_gases[0] = ID_TOTAL_OXIDIZING_GASES;
  total_oxidizing_gases[1] = (valid << 7) | LENGTH_FORMAT2;
  total_oxidizing_gases[2] = packet_format2[0];
  total_oxidizing_gases[3] = packet_format2[1];
  
  
  
  // Ambient concentration of carbon monoxide (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  carbon_monoxide[0] = ID_CARBON_MONOXIDE;
  carbon_monoxide[1] = (valid << 7) | LENGTH_FORMAT2;
  carbon_monoxide[2] = packet_format2[0];
  carbon_monoxide[3] = packet_format2[1];
  
  
  
  // Ambient concentration of sulfur dioxide (format 2)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  integer2 = random(0, 256);
  value_i = (integer1 << 8) | integer2;
  format2(value_i);  // Put it into format 2
  sulfur_dioxide[0] = ID_SULFUR_DIOXIDE;
  sulfur_dioxide[1] = (valid << 7) | LENGTH_FORMAT2;
  sulfur_dioxide[2] = packet_format2[0];
  sulfur_dioxide[3] = packet_format2[1];
  
  
  
  // Temp (format 1)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 128);
  fractional = random(0, 100);
  neg = random(0, 2);
  value_f = integer1 + ((float)fractional / 100);
  if (neg)
    value_f *= -1;
  format1(value_f);  // Put it into format 1
  sensirion[0] = ID_SENSIRION;
  sensirion[1] = (valid << 7) | (LENGTH_FORMAT1 * 2);
  sensirion[2] = packet_format1[0];
  sensirion[3] = packet_format1[1];
  
  // RH (format 1)
  integer1 = random(0, 101); // percentage
  fractional = random(0, 100);
  value_f = integer1 + ((float)fractional / 100);
  format1(value_f);  // Put it into format 1
  sensirion[4] = packet_format1[0];
  sensirion[5] = packet_format1[1];
  
  
  
  // Atmospheric pressure (format 6)
  valid = random(0, 2);
  valid_array[valid_index++] = valid;
  integer1 = random(0, 64);
  integer2 = random(0, 256);
  integer3 = random(0, 256);
  value_i = (integer1 << 16) | (integer2 << 8) | integer3;
  neg = random(0, 2);
  if (neg)
    value_i *= -1;
  format6(value_i);  // Put it into format 6
  bosh[0] = ID_BOSH;
  bosh[1] = (valid << 7) | LENGTH_FORMAT6;
  bosh[2] = packet_format6[0];
  bosh[3] = packet_format6[1];
  bosh[4] = packet_format6[2];
  
  
  
  // Intel MAC address (format 3)
  valid = 1;
  valid_array[valid_index] = valid;
  intel_MAC_ID[0] = ID_INTEL_MAC;
  intel_MAC_ID[1] = (valid << 7) | LENGTH_FORMAT3;
  intel_MAC_ID[2] = 0x07;
  intel_MAC_ID[3] = 0x08;
  intel_MAC_ID[4] = 0x09;
  intel_MAC_ID[5] = 0x0A;
  intel_MAC_ID[6] = 0x0B;
  intel_MAC_ID[7] = 0x0C;
  
  
  
  // Health status
  packet_health[0] = ID_HEALTH;
  packet_health[1] = LENGTH_HEALTH;
  int j = 0;
  for (int i = 2; i < ((LENGTH_HEALTH * 8) + 2); i++)
  {
    // Fill packet_health bit by bit
    packet_health[i] = valid_array[j] << j;
    
    j++;
  }
  
  
  
//  byte f5[LENGTH_FORMAT5];
//  valid = random(0, 2);
//  integer1 = random(0, 64);
//  integer2 = random(0, 256);
//  neg = random(0, 2);
//  value_i = (integer1 << 8) | integer2;
//  if (neg)
//    value_i *= -1;
//  format5(value_i);  // Put it into format 5
//  f5[0] = (valid << 7) | LENGTH_FORMAT5;
//  f5[1] = packet_format5[0];
//  f5[2] = packet_format5[1];
}
/**************************************************************************************/



/** Format 1 assembler ****************************************************************/
void format1(float input)
{  
  // Flag to store pos/neg info 
  byte _negative;
  
  // Input negative?
  if (input < 0) {
    _negative = 1;
  }
  else {
    _negative = 0;
  }
  
  // Get abs. value of input
  input = abs(input);
  // Extract integer component
  int integer = (int)input;
  // Extract fractional component (and turn it into an integer)
  int fractional = (input - integer) * 100;
  
  // Second byte (for integer) (1 = converted data)
  byte byte1 = (1 << 7) | integer;
  
  // Third byte (for fractional)
  byte byte2 = (_negative << 7) | fractional;
  
  // Assemble sub-packet
  packet_format1[0] = byte1;
  packet_format1[1] = byte2;
}
/**************************************************************************************/



/** Format 2 assembler ****************************************************************/
void format2(int input)
{
  // Extract MSB
  byte byte1 = input >> 8;
  
  // Extract LSB
  byte byte2 = input;
  
  // Assemble sub-packet
  packet_format2[0] = byte1;
  packet_format2[1] = byte2;
}
/**************************************************************************************/



/** Format 3 assembler ****************************************************************/
void format3(byte *input)
{
  // Assemble sub-packet
  packet_format3[0] = input[0];
  packet_format3[1] = input[1];
  packet_format3[2] = input[2];
  packet_format3[3] = input[3];
  packet_format3[4] = input[4];
  packet_format3[5] = input[5];
}
/**************************************************************************************/



/** Format 4 assembler ****************************************************************/
void format4(float input)
{
  // Flag to store pos/neg info 
  byte _negative;
  
  // Input negative?
  if (input < 0) {
    _negative = 1;
  }
  else {
    _negative = 0;
  }
  
  // Get abs. value of input
  input = abs(input);
  // Extract integer component
  int integer = (int)input;
  // Extract fractional component (and turn it into an integer)
  int fractional = (input - integer) * 1000;
  
  // Second byte
  byte byte1 = (1 << 7) | (_negative << 6); // 1 = converted data
  byte1 |= (integer << 2);  // Insert integer component
  byte1 |= (fractional >> 8); // Insert fractional component
  
  // Third byte
  byte byte2 = fractional;
  
  // Assemble sub-packet
  packet_format4[0] = byte1;
  packet_format4[1] = byte2;
}
/**************************************************************************************/



/** Format 5 assembler ****************************************************************/
void format5(int input)
{
  // Flag to store pos/neg info 
  byte _negative;
  
  // Input negative?
  if (input < 0) {
    _negative = 1;
  }
  else {
    _negative = 0;
  }
  
  // Get abs. value of input
  input = abs(input);
  
  // Second byte
  byte byte1 = _negative << 6;
  // Add 6 upper bits to complete byte 1
  byte1 |= input >> 8;
  
  // Third byte
  byte byte2 = input;
  
  // Assemble sub-packet
  packet_format5[0] = byte1;
  packet_format5[1] = byte2;
}
/**************************************************************************************/



/** Format 6 assembler ****************************************************************/
void format6(int input)
{
  // Flag to store pos/neg info 
  byte _negative;
  
  // Input negative?
  if (input < 0) {
    _negative = 1;
  }
  else {
    _negative = 0;
  }
  
  // Get abs. value of input
  input = abs(input);
  
  // Second byte
  byte byte1 = (1 << 7) | (_negative << 6); // 1 = converted data
  // Add 6 upper bits to complete byte 1
  byte1 |= input >> 16;
  
  // Third byte
  byte byte2 = input >> 8;
  
  // Fourth byte
  byte byte3 = input;
  
  // Assemble sub-packet
  packet_format6[0] = byte1;
  packet_format6[1] = byte2;
  packet_format6[2] = byte3;
}
/**************************************************************************************/



/** Assemble whole packet *************************************************************/
void assemble_packet_whole()
{
  int packet_whole_index = 3; // start at 3 to account for header
  length_data_actual = 0;
  length_whole_actual = 0;
  
  // Insert start byte
  packet_whole[0] = START_BYTE;
  // Increment length of whole packet
  length_whole_actual++;
  
  // Append reserved/version
  packet_whole[1] = HEADER_RESERVED | HEADER_VERSION;
  length_whole_actual++;    
     
  // Append MAC_ID
  for (int i = 0; i < sizeof(MAC_ID); i++)
  {
    packet_whole[packet_whole_index] = MAC_ID[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append TMP112
  for (int i = 0; i < sizeof(TMP112); i++)
  {
    packet_whole[packet_whole_index] = TMP112[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append HTU21D
  for (int i = 0; i < sizeof(HTU21D); i++)
  {
    packet_whole[packet_whole_index] = HTU21D[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append GP2Y1010AU0F
  for (int i = 0; i < sizeof(GP2Y1010AU0F); i++)
  {
    packet_whole[packet_whole_index] = GP2Y1010AU0F[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append BMP180
  for (int i = 0; i < sizeof(BMP180); i++)
  {
    packet_whole[packet_whole_index] = BMP180[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append PR103J2
  for (int i = 0; i < sizeof(PR103J2); i++)
  {
    packet_whole[packet_whole_index] = PR103J2[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append TSL250RD_1
  for (int i = 0; i < sizeof(TSL250RD_1); i++)
  {
    packet_whole[packet_whole_index] = TSL250RD_1[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append MMA8452Q
  for (int i = 0; i < sizeof(MMA8452Q); i++)
  {
    packet_whole[packet_whole_index] = MMA8452Q[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append SPV1840LR5HB_1
  for (int i = 0; i < sizeof(SPV1840LR5HB_1); i++)
  {
    packet_whole[packet_whole_index] = SPV1840LR5HB_1[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append TSYS01
  for (int i = 0; i < sizeof(TSYS01); i++)
  {
    packet_whole[packet_whole_index] = TSYS01[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append HMC5883L
  for (int i = 0; i < sizeof(HMC5883L); i++)
  {
    packet_whole[packet_whole_index] = HMC5883L[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append HIH6130
  for (int i = 0; i < sizeof(HIH6130); i++)
  {
    packet_whole[packet_whole_index] = HIH6130[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append APDS9006020
  for (int i = 0; i < sizeof(APDS9006020); i++)
  {
    packet_whole[packet_whole_index] = APDS9006020[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append TSL260RD
  for (int i = 0; i < sizeof(TSL260RD); i++)
  {
    packet_whole[packet_whole_index] = TSL260RD[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append TSL250RD_2
  for (int i = 0; i < sizeof(TSL250RD_2); i++)
  {
    packet_whole[packet_whole_index] = TSL250RD_2[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append MLX75305
  for (int i = 0; i < sizeof(MLX75305); i++)
  {
    packet_whole[packet_whole_index] = MLX75305[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append ML8511
  for (int i = 0; i < sizeof(ML8511); i++)
  {
    packet_whole[packet_whole_index] = ML8511[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append D6T
  for (int i = 0; i < sizeof(D6T); i++)
  {
    packet_whole[packet_whole_index] = D6T[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append MLX90614
  for (int i = 0; i < sizeof(MLX90614); i++)
  {
    packet_whole[packet_whole_index] = MLX90614[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append TMP421
  for (int i = 0; i < sizeof(TMP421); i++)
  {
    packet_whole[packet_whole_index] = TMP421[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append SPV1840LR5HB_2
  for (int i = 0; i < sizeof(SPV1840LR5HB_2); i++)
  {
    packet_whole[packet_whole_index] = SPV1840LR5HB_2[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append total_reducing_gases
  for (int i = 0; i < sizeof(total_reducing_gases); i++)
  {
    packet_whole[packet_whole_index] = total_reducing_gases[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append ethanol
  for (int i = 0; i < sizeof(ethanol); i++)
  {
    packet_whole[packet_whole_index] = ethanol[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append nitrogen_dioxide
  for (int i = 0; i < sizeof(nitrogen_dioxide); i++)
  {
    packet_whole[packet_whole_index] = nitrogen_dioxide[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append ozone
  for (int i = 0; i < sizeof(ozone); i++)
  {
    packet_whole[packet_whole_index] = ozone[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append hydrogen_sulphide
  for (int i = 0; i < sizeof(hydrogen_sulphide); i++)
  {
    packet_whole[packet_whole_index] = hydrogen_sulphide[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append total_oxidizing_gases
  for (int i = 0; i < sizeof(total_oxidizing_gases); i++)
  {
    packet_whole[packet_whole_index] = total_oxidizing_gases[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append carbon_monoxide
  for (int i = 0; i < sizeof(carbon_monoxide); i++)
  {
    packet_whole[packet_whole_index] = carbon_monoxide[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append sulfur_dioxide
  for (int i = 0; i < sizeof(sulfur_dioxide); i++)
  {
    packet_whole[packet_whole_index] = sulfur_dioxide[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append sensirion
  for (int i = 0; i < sizeof(sensirion); i++)
  {
    packet_whole[packet_whole_index] = sensirion[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append bosh
  for (int i = 0; i < sizeof(bosh); i++)
  {
    packet_whole[packet_whole_index] = bosh[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append intel_MAC_ID
  for (int i = 0; i < sizeof(intel_MAC_ID); i++)
  {
    packet_whole[packet_whole_index] = intel_MAC_ID[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Append health
  for (int i = 0; i < sizeof(packet_health); i++)
  {
    packet_whole[packet_whole_index] = packet_health[i];
    
    // Increment index for whole packet
    packet_whole_index++;
    
    length_whole_actual++;
    length_data_actual++;
  }
  
  // Length
  packet_whole[2] = length_data_actual;
  length_whole_actual++;
  
  // Append CRC8
  packet_whole[packet_whole_index] = CRC_calc(length_data_actual);
  length_whole_actual++;
  
  // Increment index for whole packet
  packet_whole_index++;
  
  // Append end byte
  packet_whole[packet_whole_index] = END_BYTE;
  length_whole_actual++;
}
/**************************************************************************************/



/** Assemble empty packet *************************************************************/
void assemble_packet_empty()
{
  // Insert start byte
  packet_whole[0] = START_BYTE;
  
  // Append reserved/version
  packet_whole[1] = HEADER_RESERVED | HEADER_VERSION;
  
  // Set packet length to zero
  //packet_whole[2] = 0x00;
  
  // Length of packet = just the header
  //length_whole_actual = 3;
}
/**************************************************************************************/



/** Calculate CRC, byte by byte *******************************************************/
byte CRC_8(byte data, byte crc)
{
  byte i;
  
  crc ^= data;
  
  for (i=0x00; i < 0x08; i++)
  {
    if (crc & 0x01)
      crc = (crc >> 0x01) ^ 0x8C;
    else
      crc =  crc >> 0x01;
  }

  return(crc);
} 
/**************************************************************************************/



/** Calculate CRC8 ********************************************************************/
byte CRC_calc(byte length_data)
{
  byte crc = 0x00;
  
  for (int i = 0x03; i < (length_data + 0x03); i++) // 0x03 accounts for header
    crc = CRC_8(packet_whole[i], crc);

  return crc;
} 
/**************************************************************************************/



/** I2C request interrupt *************************************************************/
void requestEvent()
{
  // Send it!
  Wire.write(packet_whole, length_whole_actual);
  
  // Assemble empty packet, in case master requests more data before its available
  assemble_packet_empty();
}
/**************************************************************************************/
