#ifndef C_DATA_H
#define C_DATA_H

#include "common_defs.h"

struct can_message_t
{
	union{
		struct{
			unsigned int id:29;
			unsigned int padding:2;
			unsigned int ext_id:1;
		};
		unsigned int msgid;
	};
	uint8_t msg[8];
	uint8_t len;
};


#endif
