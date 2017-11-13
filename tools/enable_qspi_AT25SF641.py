#!/usr/bin/env python3

import serial
import time
import sys

# serprog constants
ACK = b'\x06'
O_SPIOP = b'\x13'

# flash chip constants
READ_SR2 = b'\x35'
WRITE_ENABLE = b'\x06'
WRITE_SR2 = b'\x31'

def send_recieve(ser, data_in, data_out_length):
  ser.write(O_SPIOP + len(data_in).to_bytes(3, 'little') + data_out_length.to_bytes(3, 'little') + data_in)
  ack = ser.read(1)
  if ack != ACK:
    print('Communication Error')
    exit(1)
  return ser.read(data_out_length)

device = '/dev/ttyACM0'
if len(sys.argv) >= 2:
  device = sys.argv[1]

ser = serial.Serial(device)
print('Using: ' + ser.name)

old_sr2 = send_recieve(ser, READ_SR2, 1)
print('Status Register 2 before = ' + str(old_sr2))

new_sr2 = bytes([old_sr2[0] | 0x00000002])
print('Programming Status Register 2 with ' + str(new_sr2))
send_recieve(ser, WRITE_ENABLE, 0)
send_recieve(ser, WRITE_SR2 + new_sr2, 0)

time.sleep(1)
programmed_sr2 = send_recieve(ser, READ_SR2, 1)
print('Status Register 2 after = ' + str(programmed_sr2))

if programmed_sr2 == new_sr2:
  print('Programming done')
else:
  print('Programming error')

ser.close()
