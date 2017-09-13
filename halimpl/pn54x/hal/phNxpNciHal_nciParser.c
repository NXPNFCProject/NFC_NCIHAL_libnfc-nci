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

#include "phNxpNciHal_nciParser.h"

#include <dlfcn.h>
#include <string.h>

#include "phNxpLog.h"

typedef void* tNCI_PARSER_OBJECT;

typedef struct {
    tNCI_PARSER_OBJECT    pvHandle;
    tNCI_PARSER_FUNCTIONS sEntryFuncs;
} sParserContext_t;

sParserContext_t sParserContext;

unsigned char
phNxpNciHal_initParser() {

    char *error;
    void *lib_handle;
    sParserContext_t *psContext = &sParserContext;

    memset(&psContext->sEntryFuncs,0,sizeof(psContext->sEntryFuncs));

    psContext->pvHandle = NULL;
    psContext->sEntryFuncs.createParser = NULL;
    psContext->sEntryFuncs.initParser = NULL;
    psContext->sEntryFuncs.deinitParser = NULL;
    psContext->sEntryFuncs.destroyParser = NULL;
    psContext->sEntryFuncs.parsePacket = NULL;


    NXPLOG_NCIHAL_D("%s: enter", __FUNCTION__);

    lib_handle = dlopen(NXP_NCI_PARSER_PATH, RTLD_NOW);
    if (!lib_handle)
    {
        NXPLOG_NCIHAL_E("%s: dlopen failed !!!", __FUNCTION__);
        return FALSE;
    }

    psContext->sEntryFuncs.createParser = dlsym(lib_handle, "native_createParser");
    if (psContext->sEntryFuncs.createParser == NULL)
    {
        NXPLOG_NCIHAL_E("%s: dlsym native_createParser failed !!!", __FUNCTION__);
        return FALSE;
    }

    psContext->sEntryFuncs.destroyParser = dlsym(lib_handle, "native_destroyParser");
    if (psContext->sEntryFuncs.destroyParser == NULL)
    {
        NXPLOG_NCIHAL_E("%s: dlsym native_destroyParser failed !!!", __FUNCTION__);
        return FALSE;
    }

    psContext->sEntryFuncs.initParser = dlsym(lib_handle, "native_initParser");
    if (psContext->sEntryFuncs.initParser == NULL)
    {
        NXPLOG_NCIHAL_E("%s: dlsym native_initParser failed !!!", __FUNCTION__);
        return FALSE;
    }

    psContext->sEntryFuncs.deinitParser = dlsym(lib_handle, "native_deinitParser");
    if (psContext->sEntryFuncs.deinitParser == NULL)
    {
        NXPLOG_NCIHAL_E("%s: dlsym native_deinitParser failed !!!", __FUNCTION__);
        return FALSE;
    }

    psContext->sEntryFuncs.parsePacket = dlsym(lib_handle, "native_parseNciMsg");
    if (psContext->sEntryFuncs.parsePacket == NULL)
    {
        NXPLOG_NCIHAL_E("%s: dlsym native_parseNciMsg failed !!!", __FUNCTION__);
        return FALSE;
    }

    psContext->pvHandle = (*(psContext->sEntryFuncs.createParser))();

    if(psContext->pvHandle != NULL)
    {
        (*(psContext->sEntryFuncs.initParser))(psContext->pvHandle);
    }
    else
    {
        NXPLOG_NCIHAL_E("Parser Creation Failed !!!");
        return FALSE;
    }

    NXPLOG_NCIHAL_D("%s: exit", __FUNCTION__);

    return TRUE;
}

void
phNxpNciHal_parsePacket(unsigned char *pNciPkt, unsigned short pktLen) {

    sParserContext_t *psContext = &sParserContext;

    NXPLOG_NCIHAL_D("%s: enter", __FUNCTION__);

    if((pNciPkt == NULL) || (pktLen == 0))
    {
        NXPLOG_NCIHAL_E("Invalid NCI Packet");
        return;
    }

    if(psContext->pvHandle != NULL)
    {
        (*(psContext->sEntryFuncs.parsePacket))(psContext->pvHandle, pNciPkt, pktLen);
    }
    else
    {
        NXPLOG_NCIHAL_E("Invalid Handle");
    }
    NXPLOG_NCIHAL_D("%s: exit", __FUNCTION__);
}

void phNxpNciHal_deinitParser() {

    sParserContext_t *psContext = &sParserContext;

    NXPLOG_NCIHAL_D("%s: enter", __FUNCTION__);

    if(psContext->pvHandle != NULL)
    {
        (*(psContext->sEntryFuncs.deinitParser))(psContext->pvHandle);

        (*(psContext->sEntryFuncs.destroyParser))(psContext->pvHandle);
    }
    else
    {
        NXPLOG_NCIHAL_E("Invalid Handle");
    }

    NXPLOG_NCIHAL_D("%s: exit", __FUNCTION__);
}
