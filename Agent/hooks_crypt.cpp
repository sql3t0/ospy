/**
 * Copyright (C) 2006  Ole Andr� Vadla Ravn�s <oleavr@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "stdafx.h"
#include "hooking.h"
#include "logging.h"
#include <Wincrypt.h>
#include <psapi.h>
#include <map>

class HashContext : public BaseObject
{
protected:
    DWORD id;
    ALG_ID alg_id;

public:
    HashContext(ALG_ID alg_id)
    {
        this->id = ospy_rand();
        this->alg_id = alg_id;
    }

    DWORD get_id()
    {
        return id;
    }

    ALG_ID get_alg_id()
    {
        return alg_id;
    }

    const char *
    get_alg_id_as_string()
    {
        switch (alg_id)
        {
            case CALG_MD2: return "MD2";
            case CALG_MD4: return "MD4";
            case CALG_MD5: return "MD5";
            case CALG_SHA1: return "SHA1";
            case CALG_MAC: return "MAC";
            case CALG_RSA_SIGN: return "RSA_SIGN";
            case CALG_DSS_SIGN: return "DSS_SIGN";
            case CALG_NO_SIGN: return "NO_SIGN";
            case CALG_RSA_KEYX: return "RSA_KEYX";
            case CALG_DES: return "DES";
            case CALG_3DES_112: return "3DES_112";
            case CALG_3DES: return "3DES";
            case CALG_DESX: return "DESX";
            case CALG_RC2: return "RC2";
            case CALG_RC4: return "RC4";
            case CALG_SEAL: return "SEAL";
            case CALG_DH_SF: return "DH_SF";
            case CALG_DH_EPHEM: return "DH_EPHEM";
            case CALG_AGREEDKEY_ANY: return "AGREEDKEY_ANY";
            case CALG_KEA_KEYX: return "KEA_KEYX";
            case CALG_HUGHES_MD5: return "HUGHES_MD5";
            case CALG_SKIPJACK: return "SKIPJACK";
            case CALG_TEK: return "TEK";
            case CALG_CYLINK_MEK: return "CYLINK_MEK";
            case CALG_SSL3_SHAMD5: return "SSL3_SHAMD5";
            case CALG_SSL3_MASTER: return "SSL3_MASTER";
            case CALG_SCHANNEL_MASTER_HASH: return "SCHANNEL_MASTER_HASH";
            case CALG_SCHANNEL_MAC_KEY: return "SCHANNEL_MAC_KEY";
            case CALG_SCHANNEL_ENC_KEY: return "SCHANNEL_ENC_KEY";
            case CALG_PCT1_MASTER: return "PCT1_MASTER";
            case CALG_SSL2_MASTER: return "SSL2_MASTER";
            case CALG_TLS1_MASTER: return "TLS1_MASTER";
            case CALG_RC5: return "RC5";
            case CALG_HMAC: return "HMAC";
            case CALG_TLS1PRF: return "TLS1PRF";
            case CALG_HASH_REPLACE_OWF: return "HASH_REPLACE_OWF";
            case CALG_AES_128: return "AES_128";
            case CALG_AES_192: return "AES_192";
            case CALG_AES_256: return "AES_256";
            case CALG_AES: return "AES";
            case CALG_SHA_256: return "SHA_256";
            case CALG_SHA_384: return "SHA_384";
            case CALG_SHA_512: return "SHA_512";
            default: break;
        }

        return "UNKNOWN";
    }
};

typedef map<HCRYPTHASH, HashContext *, less<HCRYPTHASH>, MyAlloc<pair<HCRYPTHASH, HashContext *>>> HashMap;

static CRITICAL_SECTION cs;
static HashMap hash_map;

#define LOCK() EnterCriticalSection(&cs)
#define UNLOCK() LeaveCriticalSection(&cs)

static MODULEINFO schannel_info;

static BOOL
called_from_schannel(DWORD ret_addr)
{
    return (ret_addr >= (DWORD) schannel_info.lpBaseOfDll &&
            ret_addr < (DWORD) schannel_info.lpBaseOfDll + schannel_info.SizeOfImage);
}

static BOOL __cdecl
CryptCreateHash_called(BOOL carry_on,
                       DWORD ret_addr,
                       HCRYPTPROV hProv,
                       ALG_ID Algid,
                       HCRYPTKEY hKey,
                       DWORD dwFlags,
                       HCRYPTHASH *phHash)
{
    return TRUE;
}

static BOOL __stdcall
CryptCreateHash_done(BOOL retval,
                     HCRYPTPROV hProv,
                     ALG_ID Algid,
                     HCRYPTKEY hKey,
                     DWORD dwFlags,
                     HCRYPTHASH *phHash)
{
    DWORD err = GetLastError();
    int ret_addr = *((DWORD *) ((DWORD) &retval - 4));

    if (retval)
    {
        HashContext *ctx = new HashContext(Algid);

        LOCK();

        hash_map[*phHash] = ctx;

        if (!called_from_schannel(ret_addr))
        {
            message_logger_log("CryptCreateHash", ret_addr, ctx->get_id(),
                MESSAGE_TYPE_MESSAGE, MESSAGE_CTX_INFO, PACKET_DIRECTION_INVALID,
                NULL, NULL, NULL, 0,
                "hProv=0x%08x, Algid=%s, hKey=0x%08x => *phHash=0x%08x",
                hProv, ctx->get_alg_id_as_string(), hKey, *phHash);
        }

        UNLOCK();
    }

    SetLastError(err);
    return retval;
}

static BOOL __cdecl
CryptDestroyHash_called(BOOL carry_on,
                        DWORD ret_addr,
                        HCRYPTHASH hHash)
{
    return TRUE;
}

static BOOL __stdcall
CryptDestroyHash_done(BOOL retval,
                      HCRYPTHASH hHash)
{
    DWORD err = GetLastError();
    int ret_addr = *((DWORD *) ((DWORD) &retval - 4));

    if (retval)
    {
        LOCK();

        HashMap::iterator iter = hash_map.find(hHash);
        if (iter != hash_map.end())
        {
            HashContext *ctx = iter->second;

            if (!called_from_schannel(ret_addr))
            {
                message_logger_log("CryptDestroyHash", ret_addr, ctx->get_id(),
                    MESSAGE_TYPE_MESSAGE, MESSAGE_CTX_INFO, PACKET_DIRECTION_INVALID,
                    NULL, NULL, NULL, 0,
                    "hHash=0x%08x", hHash);
            }

            hash_map.erase(iter);
            delete ctx;
        }

        UNLOCK();
    }

    SetLastError(err);
    return retval;
}

static BOOL __cdecl
CryptHashData_called(BOOL carry_on,
                     DWORD ret_addr,
                     HCRYPTHASH hHash,
                     BYTE *pbData,
                     DWORD dwDataLen,
                     DWORD dwFlags)
{
    return TRUE;
}

static BOOL __stdcall
CryptHashData_done(BOOL retval,
                   HCRYPTHASH hHash,
                   BYTE *pbData,
                   DWORD dwDataLen,
                   DWORD dwFlags)
{
    DWORD err = GetLastError();
    int ret_addr = *((DWORD *) ((DWORD) &retval - 4));

    if (retval && !called_from_schannel(ret_addr))
    {
        HashMap::iterator iter;

        LOCK();

        iter = hash_map.find(hHash);
        if (iter != hash_map.end())
        {
            HashContext *ctx = iter->second;

            message_logger_log("CryptHashData", ret_addr, ctx->get_id(),
                MESSAGE_TYPE_PACKET, MESSAGE_CTX_INFO, PACKET_DIRECTION_INVALID,
                NULL, NULL, (const char *) pbData, dwDataLen,
                "hHash=0x%p, Algid=%s", hHash, ctx->get_alg_id_as_string());
        }

        UNLOCK();
    }

    SetLastError(err);
    return retval;
}

static BOOL __cdecl
CryptGetHashParam_called (BOOL carry_on,
                          DWORD ret_addr,
                          HCRYPTHASH hHash,
                          DWORD dwParam,
                          BYTE *pbData,
                          DWORD *pdwDataLen,
                          DWORD dwFlags)
{
    return TRUE;
}

static BOOL __stdcall
CryptGetHashParam_done (BOOL retval,
                        HCRYPTHASH hHash,
                        DWORD dwParam,
                        BYTE *pbData,
                        DWORD *pdwDataLen,
                        DWORD dwFlags)
{
    DWORD err = GetLastError();
    int ret_addr = *((DWORD *) ((DWORD) &retval - 4));

    if (retval && !called_from_schannel(ret_addr))
    {
        HashMap::iterator iter;

        LOCK();

        iter = hash_map.find(hHash);
        if (iter != hash_map.end())
        {
            HashContext *ctx = iter->second;
            const char *param_str;

            switch (dwParam)
            {
                case HP_ALGID:
                    param_str = "ALGID";
                    break;
                case HP_HASHSIZE:
                    param_str = "HASHSIZE";
                    break;
                case HP_HASHVAL:
                    param_str = "HASHVAL";
                    break;
                default:
                    param_str = "UNKNOWN";
                    break;
            }

            message_logger_log("CryptGetHashParam", ret_addr, ctx->get_id(),
                MESSAGE_TYPE_PACKET, MESSAGE_CTX_INFO, PACKET_DIRECTION_INVALID,
                NULL, NULL, (const char *) pbData, *pdwDataLen,
                "hHash=0x%p, Algid=%s, dwParam=%s",
                hHash, ctx->get_alg_id_as_string(), param_str);
        }

        UNLOCK();
    }

    SetLastError(err);
    return retval;
}

HOOK_GLUE_SPECIAL(CryptCreateHash, (5 * 4))
HOOK_GLUE_SPECIAL(CryptDestroyHash, (1 * 4))
HOOK_GLUE_SPECIAL(CryptHashData, (4 * 4))
HOOK_GLUE_SPECIAL(CryptGetHashParam, (5 * 4))

void
hook_crypt()
{
    InitializeCriticalSection(&cs);

    // Hook the Crypt API
    HMODULE h = LoadLibrary("advapi32.dll");
    if (h == NULL)
    {
	    MessageBox(0, "Failed to load 'advapi32.dll'.",
                   "oSpy", MB_ICONERROR | MB_OK);
        return;
    }

    HOOK_FUNCTION_SPECIAL(h, CryptCreateHash);
    HOOK_FUNCTION_SPECIAL(h, CryptDestroyHash);
    HOOK_FUNCTION_SPECIAL(h, CryptHashData);
    HOOK_FUNCTION_SPECIAL(h, CryptGetHashParam);

    h = LoadLibrary("schannel.dll");
    if (h == NULL)
    {
        MessageBox(0, "Failed to load 'schannel.dll'.",
                   "oSpy", MB_ICONERROR | MB_OK);
        return;
    }

    if (GetModuleInformation(GetCurrentProcess(), h, &schannel_info,
                             sizeof(schannel_info)) == 0)
    {
        message_logger_log_message("DllMain", 0, MESSAGE_CTX_WARNING,
                                   "GetModuleInformation failed with errno %d",
                                   GetLastError());
    }
}
