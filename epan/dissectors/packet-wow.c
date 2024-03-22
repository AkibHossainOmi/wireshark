/* packet-wow.c
 * Routines for World of Warcraft (WoW) protocol dissection
 * Copyright 2008-2009, Stephen Fisher (see AUTHORS file)
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* This dissector is based on the MaNGOS project's source code, Stanford's
 * SRP protocol documents (http://srp.stanford.edu) and RFC 2945: "The SRP
 * Authentication and Key Exchange System." */

#include "config.h"

#include <epan/packet.h>
#include <epan/prefs.h>
#include <epan/charsets.h>
#include <epan/wmem_scopes.h>
#include "packet-tcp.h"

void proto_register_wow(void);
void proto_reg_handoff_wow(void);

static dissector_handle_t wow_handle;

typedef enum {
	CMD_AUTH_LOGON_CHALLENGE       = 0x00,
	CMD_AUTH_LOGON_PROOF           = 0x01,
	CMD_AUTH_RECONNECT_CHALLENGE   = 0x02,
	CMD_AUTH_RECONNECT_PROOF       = 0x03,
    CMD_SURVEY_RESULT              = 0x04,
	CMD_REALM_LIST                 = 0x10,
	CMD_XFER_INITIATE              = 0x30,
	CMD_XFER_DATA                  = 0x31,
	CMD_XFER_ACCEPT                = 0x32,
	CMD_XFER_RESUME                = 0x33,
	CMD_XFER_CANCEL                = 0x34
} auth_cmd_e;

static const value_string cmd_vs[] = {
	{ CMD_AUTH_LOGON_CHALLENGE,       "CMD_AUTH_LOGON_CHALLENGE" },
	{ CMD_AUTH_LOGON_PROOF,           "CMD_AUTH_LOGON_PROOF" },
	{ CMD_AUTH_RECONNECT_CHALLENGE,   "CMD_AUTH_RECONNECT_CHALLENGE" },
	{ CMD_AUTH_RECONNECT_PROOF,       "CMD_AUTH_RECONNECT_PROOF" },
	{ CMD_REALM_LIST,                 "CMD_REALM_LIST" },
	{ CMD_XFER_INITIATE,              "CMD_XFER_INITIATE" },
	{ CMD_XFER_DATA,                  "CMD_XFER_DATA" },
	{ CMD_XFER_ACCEPT,                "CMD_XFER_ACCEPT" },
	{ CMD_XFER_RESUME,                "CMD_XFER_RESUME" },
	{ CMD_XFER_CANCEL,                "CMD_XFER_CANCEL" },
	{ 0, NULL }
};

/* AUTOGENERATED_START_ENUM */
typedef enum {
    REALM_TYPE_PLAYER_VS_ENVIRONMENT = 0x0,
    REALM_TYPE_PLAYER_VS_PLAYER = 0x1,
    REALM_TYPE_ROLEPLAYING = 0x6,
    REALM_TYPE_ROLEPLAYING_PLAYER_VS_PLAYER = 0x8,
} e_realm_type;
static const value_string e_realm_type_strings[] =  {
    { REALM_TYPE_PLAYER_VS_ENVIRONMENT, "Player Vs Environment" },
    { REALM_TYPE_PLAYER_VS_PLAYER, "Player Vs Player" },
    { REALM_TYPE_ROLEPLAYING, "Roleplaying" },
    { REALM_TYPE_ROLEPLAYING_PLAYER_VS_PLAYER, "Roleplaying Player Vs Player" },
    { 0, NULL }
};

typedef enum {
    REALM_CATEGORY_DEFAULT = 0x0,
    REALM_CATEGORY_ONE = 0x1,
    REALM_CATEGORY_TWO = 0x2,
    REALM_CATEGORY_THREE = 0x3,
    REALM_CATEGORY_FIVE = 0x5,
} e_realm_category;
static const value_string e_realm_category_strings[] =  {
    { REALM_CATEGORY_DEFAULT, "Default" },
    { REALM_CATEGORY_ONE, "One" },
    { REALM_CATEGORY_TWO, "Two" },
    { REALM_CATEGORY_THREE, "Three" },
    { REALM_CATEGORY_FIVE, "Five" },
    { 0, NULL }
};

typedef enum {
    PROTOCOL_VERSION_TWO = 0x2,
    PROTOCOL_VERSION_THREE = 0x3,
    PROTOCOL_VERSION_FIVE = 0x5,
    PROTOCOL_VERSION_SIX = 0x6,
    PROTOCOL_VERSION_SEVEN = 0x7,
    PROTOCOL_VERSION_EIGHT = 0x8,
} e_protocol_version;
static const value_string e_protocol_version_strings[] =  {
    { PROTOCOL_VERSION_TWO, "Two" },
    { PROTOCOL_VERSION_THREE, "Three" },
    { PROTOCOL_VERSION_FIVE, "Five" },
    { PROTOCOL_VERSION_SIX, "Six" },
    { PROTOCOL_VERSION_SEVEN, "Seven" },
    { PROTOCOL_VERSION_EIGHT, "Eight" },
    { 0, NULL }
};

typedef enum {
    PLATFORM_X86 = 0x783836,
    PLATFORM_POWER_PC = 0x505043,
} e_platform;
static const value_string e_platform_strings[] =  {
    { PLATFORM_X86, "X86" },
    { PLATFORM_POWER_PC, "Power Pc" },
    { 0, NULL }
};

typedef enum {
    OS_WINDOWS = 0x57696E,
    OS_MAC_OS_X = 0x4F5358,
} e_os;
static const value_string e_os_strings[] =  {
    { OS_WINDOWS, "Windows" },
    { OS_MAC_OS_X, "Mac Os X" },
    { 0, NULL }
};

typedef enum {
    LOCALE_EN_GB = 0x656E4742,
    LOCALE_EN_US = 0x656E5553,
    LOCALE_ES_MX = 0x65734D58,
    LOCALE_PT_BR = 0x70744252,
    LOCALE_FR_FR = 0x66724652,
    LOCALE_DE_DE = 0x64654445,
    LOCALE_ES_ES = 0x65734553,
    LOCALE_PT_PT = 0x70745054,
    LOCALE_IT_IT = 0x69744954,
    LOCALE_RU_RU = 0x72755255,
    LOCALE_KO_KR = 0x6B6F4B52,
    LOCALE_ZH_TW = 0x7A685457,
    LOCALE_EN_TW = 0x656E5457,
    LOCALE_EN_CN = 0x656E434E,
} e_locale;
static const value_string e_locale_strings[] =  {
    { LOCALE_EN_GB, "En Gb" },
    { LOCALE_EN_US, "En Us" },
    { LOCALE_ES_MX, "Es Mx" },
    { LOCALE_PT_BR, "Pt Br" },
    { LOCALE_FR_FR, "Fr Fr" },
    { LOCALE_DE_DE, "De De" },
    { LOCALE_ES_ES, "Es Es" },
    { LOCALE_PT_PT, "Pt Pt" },
    { LOCALE_IT_IT, "It It" },
    { LOCALE_RU_RU, "Ru Ru" },
    { LOCALE_KO_KR, "Ko Kr" },
    { LOCALE_ZH_TW, "Zh Tw" },
    { LOCALE_EN_TW, "En Tw" },
    { LOCALE_EN_CN, "En Cn" },
    { 0, NULL }
};

typedef enum {
    LOGIN_RESULT_SUCCESS = 0x00,
    LOGIN_RESULT_FAIL_UNKNOWN0 = 0x01,
    LOGIN_RESULT_FAIL_UNKNOWN1 = 0x02,
    LOGIN_RESULT_FAIL_BANNED = 0x03,
    LOGIN_RESULT_FAIL_UNKNOWN_ACCOUNT = 0x04,
    LOGIN_RESULT_FAIL_INCORRECT_PASSWORD = 0x05,
    LOGIN_RESULT_FAIL_ALREADY_ONLINE = 0x06,
    LOGIN_RESULT_FAIL_NO_TIME = 0x07,
    LOGIN_RESULT_FAIL_DB_BUSY = 0x08,
    LOGIN_RESULT_FAIL_VERSION_INVALID = 0x09,
    LOGIN_RESULT_LOGIN_DOWNLOAD_FILE = 0x0A,
    LOGIN_RESULT_FAIL_INVALID_SERVER = 0x0B,
    LOGIN_RESULT_FAIL_SUSPENDED = 0x0C,
    LOGIN_RESULT_FAIL_NO_ACCESS = 0x0D,
    LOGIN_RESULT_SUCCESS_SURVEY = 0x0E,
    LOGIN_RESULT_FAIL_PARENTALCONTROL = 0x0F,
    LOGIN_RESULT_FAIL_LOCKED_ENFORCED = 0x10,
} e_login_result;
static const value_string e_login_result_strings[] =  {
    { LOGIN_RESULT_SUCCESS, "Success" },
    { LOGIN_RESULT_FAIL_UNKNOWN0, "Fail Unknown0" },
    { LOGIN_RESULT_FAIL_UNKNOWN1, "Fail Unknown1" },
    { LOGIN_RESULT_FAIL_BANNED, "Fail Banned" },
    { LOGIN_RESULT_FAIL_UNKNOWN_ACCOUNT, "Fail Unknown Account" },
    { LOGIN_RESULT_FAIL_INCORRECT_PASSWORD, "Fail Incorrect Password" },
    { LOGIN_RESULT_FAIL_ALREADY_ONLINE, "Fail Already Online" },
    { LOGIN_RESULT_FAIL_NO_TIME, "Fail No Time" },
    { LOGIN_RESULT_FAIL_DB_BUSY, "Fail Db Busy" },
    { LOGIN_RESULT_FAIL_VERSION_INVALID, "Fail Version Invalid" },
    { LOGIN_RESULT_LOGIN_DOWNLOAD_FILE, "Login Download File" },
    { LOGIN_RESULT_FAIL_INVALID_SERVER, "Fail Invalid Server" },
    { LOGIN_RESULT_FAIL_SUSPENDED, "Fail Suspended" },
    { LOGIN_RESULT_FAIL_NO_ACCESS, "Fail No Access" },
    { LOGIN_RESULT_SUCCESS_SURVEY, "Success Survey" },
    { LOGIN_RESULT_FAIL_PARENTALCONTROL, "Fail Parentalcontrol" },
    { LOGIN_RESULT_FAIL_LOCKED_ENFORCED, "Fail Locked Enforced" },
    { 0, NULL }
};


typedef enum {
    REALM_FLAG_NONE = 0x00,
    REALM_FLAG_INVALID = 0x01,
    REALM_FLAG_OFFLINE = 0x02,
    REALM_FLAG_SPECIFY_BUILD = 0x04,
    REALM_FLAG_FORCE_BLUE_RECOMMENDED = 0x20,
    REALM_FLAG_FORCE_GREEN_RECOMMENDED = 0x40,
    REALM_FLAG_FORCE_RED_FULL = 0x80,
} e_realm_flag;

typedef enum {
    SECURITY_FLAG_NONE = 0x0,
    SECURITY_FLAG_PIN = 0x1,
    SECURITY_FLAG_MATRIX_CARD = 0x2,
    SECURITY_FLAG_AUTHENTICATOR = 0x4,
} e_security_flag;

typedef enum {
    ACCOUNT_FLAG_GM = 0x000001,
    ACCOUNT_FLAG_TRIAL = 0x000008,
    ACCOUNT_FLAG_PROPASS = 0x800000,
} e_account_flag;

/* AUTOGENERATED_END_ENUM */

#define WOW_PORT 3724

#define WOW_CLIENT_TO_SERVER pinfo->destport == WOW_PORT
#define WOW_SERVER_TO_CLIENT pinfo->srcport  == WOW_PORT

/* Initialize the protocol and registered fields */
static int proto_wow;


/* More than 1 packet */
static int hf_wow_command;
static int hf_wow_string_length;
/* AUTOGENERATED_START_HF */
static int hf_wow_account_flag;
static int hf_wow_account_name;
static int hf_wow_address;
static int hf_wow_authenticator;
static int hf_wow_build;
static int hf_wow_cd_key_proof;
static int hf_wow_challenge_count;
static int hf_wow_challenge_data;
static int hf_wow_checksum_salt;
static int hf_wow_client_checksum;
static int hf_wow_client_ip_address;
static int hf_wow_client_proof;
static int hf_wow_client_public_key;
static int hf_wow_compressed_data_length;
static int hf_wow_crc_hash;
static int hf_wow_crc_salt;
static int hf_wow_data;
static int hf_wow_decompressed_size;
static int hf_wow_digit_count;
static int hf_wow_error;
static int hf_wow_file_md;
static int hf_wow_file_size;
static int hf_wow_filename;
static int hf_wow_footer_padding;
static int hf_wow_game_name;
static int hf_wow_generator;
static int hf_wow_generator_length;
static int hf_wow_hardware_survey_id;
static int hf_wow_header_padding;
static int hf_wow_height;
static int hf_wow_key_count;
static int hf_wow_large_safe_prime;
static int hf_wow_large_safe_prime_length;
static int hf_wow_locale;
static int hf_wow_locked;
static int hf_wow_login_result;
static int hf_wow_major;
static int hf_wow_matrix_card_proof;
static int hf_wow_minor;
static int hf_wow_name;
static int hf_wow_number_of_characters_on_realm;
static int hf_wow_number_of_realms;
static int hf_wow_number_of_telemetry_keys;
static int hf_wow_offset;
static int hf_wow_os;
static int hf_wow_padding;
static int hf_wow_patch;
static int hf_wow_pin_grid_seed;
static int hf_wow_pin_hash;
static int hf_wow_pin_salt;
static int hf_wow_platform;
static int hf_wow_population;
static int hf_wow_proof_data;
static int hf_wow_protocol_version;
static int hf_wow_protocol_version_int;
static int hf_wow_realm_category;
static int hf_wow_realm_flag;
static int hf_wow_realm_id;
static int hf_wow_realm_type;
static int hf_wow_required;
static int hf_wow_salt;
static int hf_wow_security_flag;
static int hf_wow_seed;
static int hf_wow_server_proof;
static int hf_wow_server_public_key;
static int hf_wow_size;
static int hf_wow_survey_id;
static int hf_wow_unknown_bytes;
static int hf_wow_unknown_int;
static int hf_wow_utc_timezone_offset;
static int hf_wow_width;
/* AUTOGENERATED_END_HF */

static gboolean wow_preference_desegment = TRUE;

static gint ett_wow;
static gint ett_wow_realms;

struct game_version {
	gint8 major_version;
	gint8 minor_version;
	gint8 patch_version;
	gint16 revision;
};

static void
parse_logon_proof_client_to_server(tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset, uint8_t protocol_version) {
	guint8 two_factor_enabled;

	proto_tree_add_item(wow_tree, hf_wow_client_public_key, tvb,
			    offset, 32, ENC_NA);
	offset += 32;

	proto_tree_add_item(wow_tree, hf_wow_client_proof,
			    tvb, offset, 20, ENC_NA);
	offset += 20;

	proto_tree_add_item(wow_tree, hf_wow_crc_hash,
			    tvb, offset, 20, ENC_NA);
	offset += 20;

	proto_tree_add_item(wow_tree, hf_wow_number_of_telemetry_keys,
			    tvb, offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	if (protocol_version < 3) {
		return;
	}
	two_factor_enabled = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(wow_tree, hf_wow_security_flag, tvb,
			    offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	if (!two_factor_enabled) {
		return;
	}

	proto_tree_add_item(wow_tree, hf_wow_pin_salt, tvb,
			    offset, 16, ENC_NA);
	offset += 16;

	proto_tree_add_item(wow_tree, hf_wow_pin_hash, tvb,
			    offset, 20, ENC_NA);


}


static void
parse_logon_proof_server_to_client(tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset, uint8_t protocol_version) {
	guint8 error;

	error = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(wow_tree, hf_wow_error, tvb,
			    offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;
	if (error != LOGIN_RESULT_SUCCESS) {
		// Following fields are only present when not an error.
		return;
	}

	proto_tree_add_item(wow_tree, hf_wow_server_proof,
			    tvb, offset, 20, ENC_NA);
	offset += 20;

	if (protocol_version >= 5) {
		proto_tree_add_item(wow_tree, hf_wow_account_flag,
				    tvb, offset, 4, ENC_LITTLE_ENDIAN);
		offset += 4;
	}

	proto_tree_add_item(wow_tree, hf_wow_hardware_survey_id,
			    tvb, offset, 4, ENC_LITTLE_ENDIAN);
	offset += 4;

	if (protocol_version >= 5) {
		proto_tree_add_item(wow_tree, hf_wow_unknown_int,
				    tvb, offset, 2, ENC_LITTLE_ENDIAN);
	}
}
static void
parse_realm_list_server_to_client(packet_info *pinfo, tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset, uint8_t protocol_version) {
	guint8 num_realms, ii, number_of_realms_field_size, realm_name_offset, realm_type_field_size, realm_flags;
	gchar *string, *realm_name;
	gint len;
	proto_tree *wow_realms_tree;

	proto_tree_add_item(wow_tree, hf_wow_size,
			    tvb, offset, 2, ENC_LITTLE_ENDIAN);
	offset += 2;

	offset += 4; /* Unknown field; always 0 */

	if (protocol_version >= 8) {
		/* Possibly valid for versions starting at 2.0.0 as well */
		number_of_realms_field_size = 2;
		realm_name_offset = 3;
		realm_type_field_size = 1;
	} else {
		number_of_realms_field_size = 1;
		realm_name_offset = 5;
		realm_type_field_size = 4;
	}

	proto_tree_add_item(wow_tree, hf_wow_number_of_realms,
			    tvb, offset, number_of_realms_field_size, ENC_LITTLE_ENDIAN);
	num_realms = tvb_get_guint8(tvb, offset);
	offset += number_of_realms_field_size;

	for(ii = 0; ii < num_realms; ii++) {
		realm_name = tvb_get_stringz_enc(pinfo->pool, tvb,
						 offset + realm_name_offset,
						 &len, ENC_UTF_8);

		wow_realms_tree = proto_tree_add_subtree(wow_tree, tvb,
							 offset, 0,
							 ett_wow_realms, NULL,
							 realm_name);

		proto_tree_add_item(wow_realms_tree, hf_wow_realm_type, tvb, offset, realm_type_field_size, ENC_LITTLE_ENDIAN);
		offset += realm_type_field_size;

		if (protocol_version >= 8) {
			/* Possibly valid for versions starting at 2.0.0 as well */
			proto_tree_add_item(wow_realms_tree, hf_wow_locked, tvb, offset, 1, ENC_NA);
			offset += 1;
		}

		realm_flags = tvb_get_guint8(tvb, offset);
		proto_tree_add_item(wow_realms_tree, hf_wow_realm_flag, tvb, offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

		proto_tree_add_string(wow_realms_tree, hf_wow_name, tvb, offset, len, realm_name);
		offset += len;

		string = tvb_get_stringz_enc(pinfo->pool, tvb, offset,
					     &len, ENC_UTF_8);
		proto_tree_add_string(wow_realms_tree, hf_wow_address, tvb, offset, len, string);
		offset += len;

		proto_tree_add_item(wow_realms_tree, hf_wow_population, tvb, offset, 4, ENC_LITTLE_ENDIAN);
		offset += 4;

		proto_tree_add_item(wow_realms_tree, hf_wow_number_of_characters_on_realm, tvb, offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

		proto_tree_add_item(wow_realms_tree, hf_wow_realm_category, tvb, offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

		proto_tree_add_item(wow_realms_tree, hf_wow_realm_id, tvb, offset, 1, ENC_LITTLE_ENDIAN);
		offset += 1;

		if (protocol_version >= 8 && (realm_flags & REALM_FLAG_SPECIFY_BUILD)) {
			proto_tree_add_item(wow_realms_tree, hf_wow_major,
					    tvb, offset, 1, ENC_LITTLE_ENDIAN);
			offset += 1;

			proto_tree_add_item(wow_realms_tree, hf_wow_minor,
					    tvb, offset, 1, ENC_LITTLE_ENDIAN);
			offset += 1;

			proto_tree_add_item(wow_realms_tree, hf_wow_patch,
					    tvb, offset, 1, ENC_LITTLE_ENDIAN);
			offset += 1;

			proto_tree_add_item(wow_realms_tree, hf_wow_build, tvb,
					    offset, 2, ENC_LITTLE_ENDIAN);
			offset += 2;
		}
	}
}

static void
parse_logon_reconnect_proof(tvbuff_t *tvb, packet_info *pinfo, proto_tree *wow_tree, guint32 offset) {
	if (WOW_CLIENT_TO_SERVER) {
		proto_tree_add_item(wow_tree, hf_wow_challenge_data, tvb,
				offset, 16, ENC_NA);
		offset += 16;

		proto_tree_add_item(wow_tree, hf_wow_client_proof, tvb,
				offset, 20, ENC_NA);
		offset += 20;

		proto_tree_add_item(wow_tree, hf_wow_client_checksum, tvb,
				offset, 20, ENC_NA);
		offset += 20;

		proto_tree_add_item(wow_tree, hf_wow_number_of_telemetry_keys,
				tvb, offset, 1, ENC_LITTLE_ENDIAN);

	}
	else if (WOW_SERVER_TO_CLIENT) {
		proto_tree_add_item(wow_tree, hf_wow_error, tvb,
				offset, 1, ENC_LITTLE_ENDIAN);

	}

}

static void
parse_logon_reconnect_challenge_server_to_client(tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset) {
	guint8 error = tvb_get_guint8(tvb, offset);

	proto_tree_add_item(wow_tree, hf_wow_error, tvb,
			offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;
	if (error != LOGIN_RESULT_SUCCESS) {
		// Following fields are only present when not an error.
		return;
	}

	proto_tree_add_item(wow_tree, hf_wow_challenge_data, tvb,
			offset, 16, ENC_NA);
	offset += 16;

	proto_tree_add_item(wow_tree, hf_wow_checksum_salt, tvb,
			offset, 16, ENC_NA);
}

static void parse_logon_challenge_client_to_server(tvbuff_t *tvb,
                                                   proto_tree *wow_tree,
                                                   guint32 offset,
                                                   uint8_t *protocol_version) {
	guint8 srp_i_len;

        *protocol_version = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(wow_tree, hf_wow_protocol_version, tvb,
			offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	proto_tree_add_item(wow_tree, hf_wow_size,
			tvb, offset, 2, ENC_LITTLE_ENDIAN);
	offset += 2;

        proto_tree_add_item(wow_tree, hf_wow_game_name,
                            tvb, offset, 4, ENC_LITTLE_ENDIAN);
        offset += 4;


	proto_tree_add_item(wow_tree, hf_wow_major,
			tvb, offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	proto_tree_add_item(wow_tree, hf_wow_minor,
			tvb, offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	proto_tree_add_item(wow_tree, hf_wow_patch,
			tvb, offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	proto_tree_add_item(wow_tree, hf_wow_build, tvb,
			offset, 2, ENC_LITTLE_ENDIAN);
	offset += 2;

	proto_tree_add_item(wow_tree, hf_wow_platform,
			tvb, offset, 4, ENC_LITTLE_ENDIAN);
	offset += 4;

	proto_tree_add_item(wow_tree, hf_wow_os, tvb,
			offset, 4, ENC_LITTLE_ENDIAN);
	offset += 4;

	proto_tree_add_item(wow_tree, hf_wow_locale,
			tvb, offset, 4, ENC_LITTLE_ENDIAN);
	offset += 4;

	proto_tree_add_item(wow_tree,
			hf_wow_utc_timezone_offset,
			tvb, offset, 4, ENC_LITTLE_ENDIAN);
	offset += 4;

	proto_tree_add_item(wow_tree, hf_wow_client_ip_address, tvb,
			offset, 4, ENC_BIG_ENDIAN);
	offset += 4;

	proto_tree_add_item(wow_tree,
			hf_wow_string_length,
			tvb, offset, 1, ENC_LITTLE_ENDIAN);
	srp_i_len = tvb_get_guint8(tvb, offset);
	offset += 1;

	proto_tree_add_item(wow_tree,
			hf_wow_account_name, tvb,
			offset, srp_i_len,
			ENC_UTF_8);
}

static void
parse_logon_challenge_server_to_client(tvbuff_t *tvb, proto_tree *wow_tree, guint32 offset, uint8_t protocol_version) {
    guint8 error, srp_g_len, srp_n_len, two_factor_enabled;

	proto_tree_add_item(wow_tree, hf_wow_protocol_version, tvb,
			offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	error = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(wow_tree, hf_wow_error, tvb,
			offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;
	if (error != LOGIN_RESULT_SUCCESS) {
		// Following fields are only present when not an error.
		return;
	}

	proto_tree_add_item(wow_tree, hf_wow_server_public_key, tvb,
			offset, 32, ENC_NA);
	offset += 32;

	proto_tree_add_item(wow_tree, hf_wow_generator_length,
			tvb, offset, 1, ENC_LITTLE_ENDIAN);
	srp_g_len = tvb_get_guint8(tvb, offset);
	offset += 1;

	proto_tree_add_item(wow_tree, hf_wow_generator, tvb,
			offset, srp_g_len, ENC_NA);
	offset += srp_g_len;

	proto_tree_add_item(wow_tree, hf_wow_large_safe_prime_length,
			tvb, offset, 1, ENC_LITTLE_ENDIAN);
	srp_n_len = tvb_get_guint8(tvb, offset);
	offset += 1;

	proto_tree_add_item(wow_tree, hf_wow_large_safe_prime, tvb,
			offset, srp_n_len, ENC_NA);
	offset += srp_n_len;

	proto_tree_add_item(wow_tree, hf_wow_salt, tvb,
			offset, 32, ENC_NA);
	offset += 32;

	proto_tree_add_item(wow_tree, hf_wow_crc_salt, tvb,
			offset, 16, ENC_NA);
	offset += 16;

	if (protocol_version < 3) {
		/* The two factor fields were added in the 1.12 update. */
		return;
	}
	two_factor_enabled = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(wow_tree, hf_wow_security_flag, tvb,
			offset, 1, ENC_LITTLE_ENDIAN);
	offset += 1;

	if (!two_factor_enabled) {
		return;
	}
	proto_tree_add_item(wow_tree, hf_wow_pin_grid_seed, tvb,
			offset, 4, ENC_LITTLE_ENDIAN);
	offset += 4;

	proto_tree_add_item(wow_tree, hf_wow_pin_salt, tvb,
			offset, 16, ENC_NA);

}

static guint
get_wow_pdu_len(packet_info *pinfo, tvbuff_t *tvb, int offset, void *data _U_)
{
	gint8 size_field_offset = -1;
	guint8 cmd;
	guint16 pkt_len;

	cmd = tvb_get_guint8(tvb, offset);

	if(WOW_SERVER_TO_CLIENT && cmd == CMD_REALM_LIST)
		size_field_offset = 1;
	if(WOW_CLIENT_TO_SERVER && cmd == CMD_AUTH_LOGON_CHALLENGE)
		size_field_offset = 2;

	pkt_len = tvb_get_letohs(tvb, size_field_offset);

	return pkt_len + size_field_offset + 2;
}

static int
dissect_wow_pdu(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
	proto_item *ti;
	proto_tree *wow_tree;

	guint8 cmd;
	guint32 offset = 0;


	col_set_str(pinfo->cinfo, COL_PROTOCOL, "WOW");

	col_clear(pinfo->cinfo, COL_INFO);

	cmd = tvb_get_guint8(tvb, offset);

	col_set_str(pinfo->cinfo, COL_INFO,
			    val_to_str_const(cmd, cmd_vs,
				       "Unrecognized packet type"));

	if(!tree) {
		return tvb_captured_length(tvb);
	}

	ti = proto_tree_add_item(tree, proto_wow, tvb, 0, -1, ENC_NA);
	wow_tree = proto_item_add_subtree(ti, ett_wow);

	proto_tree_add_item(wow_tree, hf_wow_command, tvb, offset, 1,
			ENC_LITTLE_ENDIAN);
	offset += 1;

        conversation_t* conv = find_or_create_conversation(pinfo);
        uint8_t* protocol_version = (uint8_t*)conversation_get_proto_data(conv, proto_wow);
        if (protocol_version == NULL) {
            protocol_version = (uint8_t*)wmem_new0(wmem_file_scope(), uint8_t);
            conversation_add_proto_data(conv, proto_wow, protocol_version);
            // 2 is the lowest valid version.
            *protocol_version = 2;
        }

	switch(cmd) {

		case CMD_AUTH_RECONNECT_PROOF:
			parse_logon_reconnect_proof(tvb, pinfo, wow_tree, offset);

			break;

		case CMD_AUTH_RECONNECT_CHALLENGE:
			if (WOW_SERVER_TO_CLIENT) {
				parse_logon_reconnect_challenge_server_to_client(tvb, wow_tree, offset);
			} else if (WOW_CLIENT_TO_SERVER) {
				parse_logon_challenge_client_to_server(tvb, wow_tree, offset, protocol_version);
			}

			break;

		case CMD_AUTH_LOGON_CHALLENGE :
			if(WOW_CLIENT_TO_SERVER) {
				parse_logon_challenge_client_to_server(tvb, wow_tree, offset, protocol_version);
			} else if(WOW_SERVER_TO_CLIENT) {
				parse_logon_challenge_server_to_client(tvb, wow_tree, offset, *protocol_version);
			}

			break;

		case CMD_AUTH_LOGON_PROOF :
			if (WOW_CLIENT_TO_SERVER) {
				parse_logon_proof_client_to_server(tvb, wow_tree, offset, *protocol_version);
			} else if (WOW_SERVER_TO_CLIENT) {
				parse_logon_proof_server_to_client(tvb, wow_tree, offset, *protocol_version);
			}

			break;

		case CMD_REALM_LIST :
			if(WOW_CLIENT_TO_SERVER) {

			} else if(WOW_SERVER_TO_CLIENT) {
				parse_realm_list_server_to_client(pinfo, tvb, wow_tree, offset, *protocol_version);

			}

			break;
	}

	return tvb_captured_length(tvb);
}


static gboolean
dissect_wow(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
	gint8 size_field_offset = -1;
	guint8 cmd;

	cmd = tvb_get_guint8(tvb, 0);

	if(WOW_SERVER_TO_CLIENT && cmd == CMD_REALM_LIST)
		size_field_offset = 1;
	if(WOW_CLIENT_TO_SERVER && cmd == CMD_AUTH_LOGON_CHALLENGE)
		size_field_offset = 2;

	if(size_field_offset > -1) {
		tcp_dissect_pdus(tvb, pinfo, tree, wow_preference_desegment,
				 size_field_offset+2, get_wow_pdu_len,
				 dissect_wow_pdu, data);

	} else {
		/* Doesn't have a size field, so it cannot span multiple
		   segments.  Therefore, dissect this packet normally. */
		dissect_wow_pdu(tvb, pinfo, tree, data);
	}

	return TRUE;
}


void
proto_register_wow(void)
{
	module_t *wow_module; /* For our preferences */

	static hf_register_info hf[] = {
        { &hf_wow_command,
          { "Command", "wow.cmd",
            FT_UINT8, BASE_HEX, VALS(cmd_vs), 0,
            "Type of packet", HFILL }
        },
        { &hf_wow_string_length,
            { "String Length", "wow.string.length",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                "Length of following string", HFILL }
        },
        /* AUTOGENERATED_START_REGISTER */
        { &hf_wow_account_flag,
            { "Account Flag", "wow.account.flag",
                FT_UINT32, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_account_name,
            { "Account Name", "wow.account.name",
                FT_STRINGZ, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_address,
            { "Address", "wow.address",
                FT_STRINGZ, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_authenticator,
            { "Authenticator", "wow.authenticator",
                FT_STRINGZ, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_build,
            { "Build", "wow.build",
                FT_UINT16, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_cd_key_proof,
            { "Cd Key Proof", "wow.cd.key.proof",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_challenge_count,
            { "Challenge Count", "wow.challenge.count",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_challenge_data,
            { "Challenge Data", "wow.challenge.data",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_checksum_salt,
            { "Checksum Salt", "wow.checksum.salt",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_client_checksum,
            { "Client Checksum", "wow.client.checksum",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_client_ip_address,
            { "Client Ip Address", "wow.client.ip.address",
                FT_UINT32, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_client_proof,
            { "Client Proof", "wow.client.proof",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_client_public_key,
            { "Client Public Key", "wow.client.public.key",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_compressed_data_length,
            { "Compressed Data Length", "wow.compressed.data.length",
                FT_UINT16, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_crc_hash,
            { "Crc Hash", "wow.crc.hash",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_crc_salt,
            { "Crc Salt", "wow.crc.salt",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_data,
            { "Data", "wow.data",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_decompressed_size,
            { "Decompressed Size", "wow.decompressed.size",
                FT_UINT32, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_digit_count,
            { "Digit Count", "wow.digit.count",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_error,
            { "Error", "wow.error",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_file_md,
            { "File Md", "wow.file.md",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_file_size,
            { "File Size", "wow.file.size",
                FT_UINT64, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_filename,
            { "Filename", "wow.filename",
                FT_STRINGZ, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_footer_padding,
            { "Footer Padding", "wow.footer.padding",
                FT_UINT16, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_game_name,
            { "Game Name", "wow.game.name",
                FT_UINT32, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_generator,
            { "Generator", "wow.generator",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_generator_length,
            { "Generator Length", "wow.generator.length",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_hardware_survey_id,
            { "Hardware Survey Id", "wow.hardware.survey.id",
                FT_UINT32, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_header_padding,
            { "Header Padding", "wow.header.padding",
                FT_UINT32, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_height,
            { "Height", "wow.height",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_key_count,
            { "Key Count", "wow.key.count",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_large_safe_prime,
            { "Large Safe Prime", "wow.large.safe.prime",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_large_safe_prime_length,
            { "Large Safe Prime Length", "wow.large.safe.prime.length",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_locale,
            { "Locale", "wow.locale",
                FT_UINT32, BASE_HEX_DEC, VALS(e_locale_strings), 0,
                NULL, HFILL
            }
        },
        { &hf_wow_locked,
            { "Locked", "wow.locked",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_login_result,
            { "Login Result", "wow.login.result",
                FT_UINT8, BASE_HEX_DEC, VALS(e_login_result_strings), 0,
                NULL, HFILL
            }
        },
        { &hf_wow_major,
            { "Major", "wow.major",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_matrix_card_proof,
            { "Matrix Card Proof", "wow.matrix.card.proof",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_minor,
            { "Minor", "wow.minor",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_name,
            { "Name", "wow.name",
                FT_STRINGZ, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_number_of_characters_on_realm,
            { "Number Of Characters On Realm", "wow.number.of.characters.on.realm",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_number_of_realms,
            { "Number Of Realms", "wow.number.of.realms",
                FT_UINT16, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_number_of_telemetry_keys,
            { "Number Of Telemetry Keys", "wow.number.of.telemetry.keys",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_offset,
            { "Offset", "wow.offset",
                FT_UINT64, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_os,
            { "Os", "wow.os",
                FT_UINT32, BASE_HEX_DEC, VALS(e_os_strings), 0,
                NULL, HFILL
            }
        },
        { &hf_wow_padding,
            { "Padding", "wow.padding",
                FT_UINT32, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_patch,
            { "Patch", "wow.patch",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_pin_grid_seed,
            { "Pin Grid Seed", "wow.pin.grid.seed",
                FT_UINT32, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_pin_hash,
            { "Pin Hash", "wow.pin.hash",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_pin_salt,
            { "Pin Salt", "wow.pin.salt",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_platform,
            { "Platform", "wow.platform",
                FT_UINT32, BASE_HEX_DEC, VALS(e_platform_strings), 0,
                NULL, HFILL
            }
        },
        { &hf_wow_population,
            { "Population", "wow.population",
                FT_FLOAT, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_proof_data,
            { "Proof Data", "wow.proof.data",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_protocol_version,
            { "Protocol Version", "wow.protocol.version",
                FT_UINT8, BASE_HEX_DEC, VALS(e_protocol_version_strings), 0,
                NULL, HFILL
            }
        },
        { &hf_wow_protocol_version_int,
            { "Protocol Version Int", "wow.protocol.version.int",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_realm_category,
            { "Realm Category", "wow.realm.category",
                FT_UINT8, BASE_HEX_DEC, VALS(e_realm_category_strings), 0,
                NULL, HFILL
            }
        },
        { &hf_wow_realm_flag,
            { "Realm Flag", "wow.realm.flag",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_realm_id,
            { "Realm Id", "wow.realm.id",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_realm_type,
            { "Realm Type", "wow.realm.type",
                FT_UINT8, BASE_HEX_DEC, VALS(e_realm_type_strings), 0,
                NULL, HFILL
            }
        },
        { &hf_wow_required,
            { "Required", "wow.required",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_salt,
            { "Salt", "wow.salt",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_security_flag,
            { "Security Flag", "wow.security.flag",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_seed,
            { "Seed", "wow.seed",
                FT_UINT64, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_server_proof,
            { "Server Proof", "wow.server.proof",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_server_public_key,
            { "Server Public Key", "wow.server.public.key",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_size,
            { "Size", "wow.size",
                FT_UINT16, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_survey_id,
            { "Survey Id", "wow.survey.id",
                FT_UINT32, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_unknown_bytes,
            { "Unknown Bytes", "wow.unknown.bytes",
                FT_BYTES, BASE_NONE, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_unknown_int,
            { "Unknown Int", "wow.unknown.int",
                FT_UINT32, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_utc_timezone_offset,
            { "Utc Timezone Offset", "wow.utc.timezone.offset",
                FT_UINT32, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
        { &hf_wow_width,
            { "Width", "wow.width",
                FT_UINT8, BASE_HEX_DEC, NULL, 0,
                NULL, HFILL
            }
        },
/* AUTOGENERATED_END_REGISTER */
	};

	static gint *ett[] = {
		&ett_wow,
		&ett_wow_realms
	};

	proto_wow = proto_register_protocol("World of Warcraft",
					    "WOW", "wow");

	proto_register_field_array(proto_wow, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));

	wow_handle = register_dissector("wow", dissect_wow, proto_wow);

	wow_module = prefs_register_protocol(proto_wow, NULL);

	prefs_register_bool_preference(wow_module, "desegment", "Reassemble wow messages spanning multiple TCP segments.", "Whether the wow dissector should reassemble messages spanning multiple TCP segments.  To use this option, you must also enable \"Allow subdissectors to reassemble TCP streams\" in the TCP protocol settings.", &wow_preference_desegment);

}

void
proto_reg_handoff_wow(void)
{
	dissector_add_uint_with_preference("tcp.port", WOW_PORT, wow_handle);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */

/* AUTOGENERATED_START_VARIABLES */
/* AUTOGENERATED_END_VARIABLES */

/* AUTOGENERATED_START_PARSER */
/* AUTOGENERATED_END_PARSER */

