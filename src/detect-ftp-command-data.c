/* Copyright (C) 2025 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 *
 * \author Jeff Lucovsky <jlucovsky@oisf.net>
 *
 * Implements the ftp.command_data sticky buffer
 *
 */

#include "suricata-common.h"
#include "detect.h"

#include "detect-parse.h"
#include "detect-engine.h"
#include "detect-engine-buffer.h"
#include "detect-engine-mpm.h"
#include "detect-engine-prefilter.h"
#include "detect-engine-helper.h"
#include "detect-content.h"

#include "flow.h"

#include "util-debug.h"

#include "app-layer.h"
#include "app-layer-ftp.h"

#include "detect-ftp-command-data.h"

#define KEYWORD_NAME "ftp.command_data"
#define KEYWORD_DOC  "ftp-keywords.html#ftp-command_data"
#define BUFFER_NAME  "ftp.command_data"
#define BUFFER_DESC  "ftp command_data"

static int g_ftp_cmd_data_buffer_id = 0;

static int DetectFtpCommandDataSetup(DetectEngineCtx *de_ctx, Signature *s, const char *str)
{
    if (SCDetectBufferSetActiveList(de_ctx, s, g_ftp_cmd_data_buffer_id) < 0)
        return -1;

    if (DetectSignatureSetAppProto(s, ALPROTO_FTP) < 0)
        return -1;

    return 0;
}

static bool DetectFTPCommandDataGetData(
        void *txv, const uint8_t _flow_flags, const uint8_t **buffer, uint32_t *buffer_len)
{
    FTPTransaction *tx = (FTPTransaction *)txv;

    if (tx->command_descriptor.command_code == FTP_COMMAND_UNKNOWN)
        return false;

    const char *b;
    uint8_t b_len;
    if (SCGetFtpCommandInfo(tx->command_descriptor.command_index, &b, NULL, &b_len)) {
        if ((tx->request_length - b_len - 1) > 0) {
            // command data starts here: advance past command + 1 space
            *buffer = tx->request + b_len + 1;
            *buffer_len = tx->request_length - b_len - 1;
            SCLogDebug("command data: \"%s\" [bytes %d]", *buffer, *buffer_len);
            return true;
        }
    }

    *buffer = NULL;
    *buffer_len = 0;
    return false;
}

static InspectionBuffer *GetDataWrapper(DetectEngineThreadCtx *det_ctx,
        const DetectEngineTransforms *transforms, Flow *_f, const uint8_t _flow_flags, void *txv,
        const int list_id)
{
    return DetectHelperGetData(
            det_ctx, transforms, _f, _flow_flags, txv, list_id, DetectFTPCommandDataGetData);
}

void DetectFtpCommandDataRegister(void)
{
    /* ftp.command sticky buffer */
    sigmatch_table[DETECT_FTP_COMMAND_DATA].name = KEYWORD_NAME;
    sigmatch_table[DETECT_FTP_COMMAND_DATA].desc =
            "sticky buffer to match on the FTP command data buffer";
    sigmatch_table[DETECT_FTP_COMMAND_DATA].url = "/rules/" KEYWORD_DOC;
    sigmatch_table[DETECT_FTP_COMMAND_DATA].Setup = DetectFtpCommandDataSetup;
    sigmatch_table[DETECT_FTP_COMMAND_DATA].flags |= SIGMATCH_NOOPT;

    DetectHelperBufferMpmRegister(
            BUFFER_NAME, BUFFER_NAME, ALPROTO_FTP, STREAM_TOSERVER, GetDataWrapper);

    DetectBufferTypeSetDescriptionByName(BUFFER_NAME, BUFFER_DESC);

    g_ftp_cmd_data_buffer_id = DetectBufferTypeGetByName(BUFFER_NAME);

    SCLogDebug("registering " BUFFER_NAME " rule option");
}
