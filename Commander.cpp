// 
// Commander class for processing commands
// https://github.com/gcsalzburg
// © George Cave 2020
//

#include "Commander.h"

Commander::Commander(char *n_id, char *b_id){
	strcpy(network_id, n_id);
	strcpy(board_id, b_id);
}

void Commander::init(float freq, int8_t power){
	pinMode(RFM95_RST, OUTPUT);
	digitalWrite(RFM95_RST, HIGH);
	delay(100);

	// manual reset
	digitalWrite(RFM95_RST, LOW);
	delay(10);
	digitalWrite(RFM95_RST, HIGH);
	delay(10);

	// Check if init was possible
	while (!rf95.init()) {
		Serial.println("LoRa radio init failed");
		Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
		while (1);
	}
	Serial.println("LoRa radio init!");

	// Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
	if (!rf95.setFrequency(freq)) {
		Serial.println("setFrequency failed");
		while (1);
	}
	Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

	// Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

	// The default transmitter power is 13dBm, using PA_BOOST.
	// If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
	// you can set transmitter powers from 5 to 23 dBm:
	rf95.setTxPower(power, false);

	// Send a bootup message
	send_boot_msg();
}

bool Commander::available(){
	// Read in incoming data
	if (rf95.available()){
		
		uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
		uint8_t len = sizeof(buf);

		if (rf95.recv(buf, &len)){
			// Read in string to commander!
			if(read(buf, len)){
				return true;
			}
		}else{
			// Receive must have failed, for some reason
		}
	}
	return false;
}

//
// Reads in one character at a time, e.g. from a Serial reader
// Will return true if we have a new message to read
bool Commander::read(char c){
	
	switch (c){
		case 0:	// Ignore NUL chars
			break;
		case '\r': // Carriage return move to beginning of next line
		case '\n': // Line feed move one line forward
			return process_input();
			break;
		default:
			buffer[buffer_i] = c;
			buffer_i++;
			if(buffer_i > strlen(buffer)){
				// Quit and process if we get too much input
				return process_input();
			}
	}
	return false;
}

//
// Reads in a whole buffer length at a time, e.g. from LoRa radio
// Will return true if we have a new message to read
bool Commander::read(uint8_t *buff, uint8_t len){

	strcpy(buffer, (char *)buff);
	return process_input();
}

//
// Strip out message into parts for processing
// Expected message format 
// RRx.########
// |2 character network ID
//   |1 character board ID
//    |Divider
//     |Message contents
bool Commander::process_input(){

	if(strlen(buffer) < 5){
		//No actual message provided
		cleanup();
		return false;
	}
	if((buffer[0] != network_id[0]) || (buffer[1] != network_id[1])){
		// First chars do not match
		cleanup();
		return false;
	}
	if(buffer[2] != board_id[0]){
		// Not a packet for this board!
		cleanup();
		return false;
	}
	if(buffer[3] != '.'){
		// No point separator
		cleanup();
		return false;
	}

	// Save message ready for retrieval
	memset(msg, 0, sizeof msg);	
	msg_length = strlen(buffer)-4;			
	strncpy(msg, &buffer[4], msg_length);

	cleanup();
	return true;
}

void Commander::send(char *msg, uint8_t len){
	send(msg, len, board_id);
}

void Commander::send(char *msg, uint8_t len, char *b_id){

	char buffer[buffer_size+1];

	// Assemble packet in expected format
	// ##x.###########
	strncpy(buffer, network_id, 2); 
	strncpy(&buffer[2], b_id, 1); 
	buffer[3] = '.';
	strncpy(&buffer[4], msg, len); 	

	// Send packet!
	rf95.send((uint8_t *)buffer, len+4);
	rf95.waitPacketSent();

	last_send = millis();
}

void Commander::ping(){
	if(millis() >= last_send+ping_interval){
		char packet_msg[] = "9";
		send(packet_msg, 2);
	}
}

// //////////////////////////////////////

void Commander::send_boot_msg(){
	char packet_msg[] = "1";
	send(packet_msg, 2);
}

void Commander::cleanup(){
	memset(buffer, 0, sizeof buffer);
	buffer_i = 0;
}