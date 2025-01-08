/*
 * Copyright 2010-2018, 2020-2021, 2025 NXP
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
 * NFC Status Values - Function Return Codes
 */

#ifndef PHNFCSTATUS_H
#define PHNFCSTATUS_H

#include <phNfcTypes.h>

/*
 *  Status Codes
 *
 *  Generic Status codes for the NFC components. Combined with the Component ID
 *  they build the value (status) returned by each function.
 *  Example:
 *      grp_comp_id "Component ID" -  e.g. 0x10, plus
 *      status code as listed in this file - e.g. 0x03
 *      result in a status value of 0x0003.
 */

/*
 * The function indicates successful completion
 */
#define NFCSTATUS_SUCCESS (0x0000)

/*
 *  The function indicates successful completion
 */
#define NFCSTATUS_OK (NFCSTATUS_SUCCESS)

/*
 * Response Time out for the control message(NFCC not responded)
 */
#define NFCSTATUS_RESPONSE_TIMEOUT (0x0025)

/*
 * Status code for failure
 */
#define NFCSTATUS_FAILED (0x00FF)

/*
 * Indicates, it is handled as part of extenstion feature
 */
#define NFCSTATUS_EXTN_FEATURE_SUCCESS (0x0050)

/*
 * Indicates, it is not handled as part of extenstion feature
 */
#define NFCSTATUS_EXTN_FEATURE_FAILURE (0x0051)

#endif /* PHNFCSTATUS_H */
