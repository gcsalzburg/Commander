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
static const uint8_t BUFFER_SIZE = 128;

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
			READING,
			RECEIVING,
			SENDING,
			AWAITING_RESPONSE,
			NO_RESPONSE,
			PING_START
		};

		Commander(char *network_id, char *board_id);

      void setStatusCallback(void (*status_callback)(Commander::status));

		// Initialise the LoRa radio etc...
		void init(float freq = RF95_FREQ, int8_t power = 23);

		// Check if message is available for processing
		bool available();

		// Sends message to network
		void send(char *msg, uint8_t len, bool request_reply = true);
		void send(char *msg, char *board_id, uint8_t len, bool request_reply = true);

		// Checks if we haven't sent a message for a while, and if so will ping
		// Safe to call as often as possible
		void ping();

		char 	  msg[BUFFER_SIZE+1];
		uint8_t msg_length;
		char    msg_rand[4];

	private:
	
		RH_RF95 rf95 = RH_RF95(RFM95_CS, RFM95_INT);

		// Sent at the start of every packet
		char _network_id[2] = "";
		char _board_id[1] = "";

		uint32_t _last_send = 0;

		// Read in message
		bool _read(uint8_t *buff, uint8_t len);

		void _send(char *msg, uint8_t len, bool do_retry, bool is_ack);
		void _send(char *msg, char *board_id, uint8_t len,  bool do_retry, bool is_ack);

		void _send_boot_msg();

		bool _process_input(bool is_ack_check = false);
		void _cleanup();
	
		char _buffer[BUFFER_SIZE+1]; 	
		uint16_t _buffer_i = 0;  
      
		// Callback to use when send/receive state changes
		void _status_change(Commander::status new_status);
		void (*_status_callback)(Commander::status);

		
		// Interval between keep-alive messages
		const uint32_t _ping_interval = 5000;

		// Resend delay
		const uint16_t _resend_delay = 300;
		const uint8_t _max_retries = 3;
};

#endif