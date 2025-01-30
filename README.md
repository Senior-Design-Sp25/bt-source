This repository stores the code for the esp32 source device. It is a modified version of the espressif bt classic A2DP source example.

The source device receives incoming audio from the sink esp over I2S, modulates volume, and passes it to bt to be transmitted to the receiver.

I2S AUDIO --> VOLUME MODULATION --> RINGBUFF --> BT SOURCE TRANSMISSION