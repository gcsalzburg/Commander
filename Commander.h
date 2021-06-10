// 
// Commander class for processing commands
// https://github.com/gcsalzburg
// © George Cave 2021
//

#ifndef Commander_H_
#define Commander_H_

#include <Arduino.h>
#include <SPI.h>
#include <RH_RF95.h>

// Setup for LoRa radio
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
#define RF95_FREQ 868.0

// Incoming command buffer
static const uint8_t buffer_size = 128;

// Interval between keep-alive messages
const uint32_t ping_interval = 5000;

// Resend delay
const uint8_t resend_delay = 2000;
const uint8_t max_retries = 3;

const char alphanumeric[] = {"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"};

//
// Process incoming commands in simple way
// Format:
// 0nXXXXXX
// ^--Object index to operate on, from 0 to 9
//  ^--Command character
//   ^--Data (as long as needed for the command)
//
class Commander{

	public: 

		enum status{
			OK,
			ERROR,
			RECEIVING,
			SENDING,
			AWAITING_RESPONSE,
			NO_RESPONSE,
			PING_START
		};

		Commander(char *n_id, char *b_id);

      void setStatusCallback(void (*status_callback)(Commander::status));

		void init(float freq = RF95_FREQ, int8_t power = 23);

		// Read in message
		bool read(char c);
		bool read(uint8_t *buff, uint8_t len);

		// Check if message is available for processing
		bool available();

		// Sends message to network
		void send(char *msg, uint8_t len, bool request_reply = true);
		void send(char *msg, char *b_id, uint8_t len, bool request_reply = true);

		// Checks if we haven't sent a message for a while, and if so will ping
		// Safe to call as often as possible
		void ping();

		char 	  msg[buffer_size+1];
		uint8_t msg_length;

	private:
	
		RH_RF95 rf95 = RH_RF95(RFM95_CS, RFM95_INT);

		// Sent at the start of every packet
		char network_id[2] = "";
		char board_id[1] = "";

		uint32_t last_send = 0;

		void _send(char *msg, uint8_t len, bool do_retry, bool is_ack);
		void _send(char *msg, char *b_id, uint8_t len,  bool do_retry, bool is_ack);

		void send_boot_msg();

		bool process_input();
		void cleanup();
	
		char buffer[buffer_size+1]; 	
		uint16_t buffer_i = 0;  
      
		// Callback to use when send/receive state changes
		void status_change(Commander::status new_status);
		void (*_status_callback)(Commander::status);
};

#endif