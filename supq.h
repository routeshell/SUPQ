#ifndef SUPQ_H
#define SUPQ_H

#include <stdint.h>

// Frame Types
#define SUPQ_FRAME_TYPE_QUERY_UPDATE   0x01
#define SUPQ_FRAME_TYPE_INITIATE_UPDATE 0x02
#define SUPQ_FRAME_TYPE_STATUS_REPORT  0x03
#define SUPQ_FRAME_TYPE_UPDATE_DATA    0x04

// Update Types
#define SUPQ_UPDATE_TYPE_SECURITY_PATCH 0x00
#define SUPQ_UPDATE_TYPE_BUG_FIX        0x01
#define SUPQ_UPDATE_TYPE_MAJOR_UPGRADE  0x02

// Status Codes
#define SUPQ_STATUS_SUCCESS             0x00
#define SUPQ_STATUS_CHECKSUM_ERROR      0x01
#define SUPQ_STATUS_INCOMPATIBLE_UPDATE 0x02
#define SUPQ_STATUS_PARTIAL_FAILED      0x03

// Frame Structure
typedef struct {
    uint8_t type;
    uint32_t length;
    uint8_t payload[];
} SUPQ_Frame;

// Query Update Payload
typedef struct {
    uint32_t software_version_length;
    uint8_t update_type;
    char software_version[]; // Move this to the end
} SUPQ_QueryUpdatePayload;

// Initiate Update Payload
typedef struct {
    uint32_t num_updates;
    uint32_t update_ids[];
} SUPQ_InitiateUpdatePayload;

// Status Report Payload
typedef struct {
    uint32_t update_id;
    uint32_t status_code;
} SUPQ_StatusReportPayload;

// Update Data Payload
typedef struct {
    uint32_t update_id;
    uint32_t chunk_number;
    uint8_t is_final_chunk;
    uint8_t data_chunk[]; // Move this to the end
} SUPQ_UpdateDataPayload;

#endif // SUPQ_H

