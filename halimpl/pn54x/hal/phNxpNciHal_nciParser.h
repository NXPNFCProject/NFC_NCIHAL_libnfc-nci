/*
 * Copyright (C) 2017 NXP Semiconductors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * phNxpNciHal_nciParser.h
 */

#ifndef _PHNXPNCIHAL_NCIPARSER_H_
#define _PHNXPNCIHAL_NCIPARSER_H_

#define NXP_NCI_PARSER_PATH "/system/lib64/libnxp_nciparser.so"

typedef void* (*tHAL_API_NATIVE_CREATE_PARSER)();
typedef void  (*tHAL_API_NATIVE_DESTROY_PARSER)(void*);
typedef void  (*tHAL_API_NATIVE_INIT_PARSER)(void*);
typedef void  (*tHAL_API_NATIVE_DEINIT_PARSER)(void*);
typedef void  (*tHAL_API_NATIVE_PARSE_PACKET)(void*,unsigned char *, unsigned short);

typedef struct
{
    tHAL_API_NATIVE_CREATE_PARSER  createParser;
    tHAL_API_NATIVE_DESTROY_PARSER destroyParser;
    tHAL_API_NATIVE_INIT_PARSER    initParser;
    tHAL_API_NATIVE_DEINIT_PARSER  deinitParser;
    tHAL_API_NATIVE_PARSE_PACKET   parsePacket;
} tNCI_PARSER_FUNCTIONS;

unsigned char phNxpNciHal_initParser();
void phNxpNciHal_parsePacket(unsigned char*, unsigned short);
void phNxpNciHal_deinitParser();

#endif /* _PHNXPNCIHAL_NCIPARSER_H_ */
