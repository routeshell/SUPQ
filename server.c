#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msquic.h"
#include "supq.h"

// Ensure MsQuicOpen is declared properly
extern const QUIC_API_TABLE* MsQuic;
QUIC_STATUS MsQuicOpen(const QUIC_API_TABLE** QuicApi);

#define SERVER_CERT_FILE "server_cert.pem"
#define SERVER_KEY_FILE  "server_key.pem"

#ifndef ARRAYSIZE
#define ARRAYSIZE(A) (sizeof(A) / sizeof((A)[0]))
#endif

const QUIC_API_TABLE* MsQuic;
HQUIC Registration;
HQUIC Configuration;
HQUIC Listener;

QUIC_STATUS QUIC_API StreamCallback(HQUIC Stream, void* Context, QUIC_STREAM_EVENT* Event);
QUIC_STATUS QUIC_API ConnectionCallback(HQUIC Connection, void* Context, QUIC_CONNECTION_EVENT* Event);
QUIC_STATUS QUIC_API ListenerCallback(HQUIC Listener, void* Context, QUIC_LISTENER_EVENT* Event);

void InitializeMsQuic() {
    printf("Initializing MsQuic...\n");

    if (QUIC_FAILED(MsQuicOpen2(&MsQuic))) {
        printf("MsQuicOpen failed\n");
        exit(1);
    }

    if (MsQuic == NULL) {
        printf("MsQuic is NULL\n");
        exit(1);
    }

    printf("MsQuicOpen successful\n");

    QUIC_REGISTRATION_CONFIG RegConfig = { "SUPQServer", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    if (QUIC_FAILED(MsQuic->RegistrationOpen(&RegConfig, &Registration))) {
        printf("RegistrationOpen failed\n");
        exit(1);
    }

    if (Registration == NULL) {
        printf("Registration is NULL\n");
        exit(1);
    }

    printf("RegistrationOpen successful\n");

    QUIC_BUFFER AlpnBuffers[] = { { sizeof("supq") - 1, (uint8_t*)"supq" } };
    QUIC_SETTINGS Settings = { 0 };

    QUIC_CREDENTIAL_CONFIG CredConfig;
    memset(&CredConfig, 0, sizeof(CredConfig));
    CredConfig.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
    CredConfig.CertificateFile = (QUIC_CERTIFICATE_FILE*)malloc(sizeof(QUIC_CERTIFICATE_FILE));
    if (CredConfig.CertificateFile == NULL) {
        printf("Memory allocation for CertificateFile failed\n");
        exit(1);
    }
    CredConfig.CertificateFile->CertificateFile = SERVER_CERT_FILE;
    CredConfig.CertificateFile->PrivateKeyFile = SERVER_KEY_FILE;

    printf("Opening configuration...\n");
    if (QUIC_FAILED(MsQuic->ConfigurationOpen(Registration, AlpnBuffers, ARRAYSIZE(AlpnBuffers), &Settings, sizeof(Settings), NULL, &Configuration))) {
        printf("ConfigurationOpen failed\n");
        exit(1);
    }

    if (Configuration == NULL) {
        printf("Configuration is NULL\n");
        exit(1);
    }

    printf("ConfigurationOpen successful\n");

    printf("Loading credentials...\n");
    if (QUIC_FAILED(MsQuic->ConfigurationLoadCredential(Configuration, &CredConfig))) {
        printf("ConfigurationLoadCredential failed\n");
        exit(1);
    }

    printf("ConfigurationLoadCredential successful\n");
}

void CleanupMsQuic() {
    if (Listener) {
        MsQuic->ListenerClose(Listener);
    }
    if (Configuration) {
        MsQuic->ConfigurationClose(Configuration);
    }
    if (Registration) {
        MsQuic->RegistrationClose(Registration);
    }
    if (MsQuic) {
        MsQuicClose(MsQuic);
    }
}

QUIC_STATUS QUIC_API ListenerCallback(HQUIC Listener, void* Context, QUIC_LISTENER_EVENT* Event) {
    switch (Event->Type) {
        case QUIC_LISTENER_EVENT_NEW_CONNECTION:
            printf("New connection\n");
            MsQuic->SetCallbackHandler(Event->NEW_CONNECTION.Connection, (void*)ConnectionCallback, NULL);
            if (QUIC_FAILED(MsQuic->ConnectionSetConfiguration(Event->NEW_CONNECTION.Connection, Configuration))) {
                printf("ConnectionSetConfiguration failed\n");
                MsQuic->ConnectionClose(Event->NEW_CONNECTION.Connection);
            }
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API ConnectionCallback(HQUIC Connection, void* Context, QUIC_CONNECTION_EVENT* Event) {
    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            printf("Connection connected\n");
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            printf("Connection shut down\n");
            MsQuic->ConnectionClose(Connection);
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

void HandleUpdateQuery(HQUIC Stream, SUPQ_Frame* frame) {
    printf("Handling update query\n");

    // Respond with update information
    const char* update_info = "Update available: v1.1.0";
    size_t update_info_len = strlen(update_info);

    SUPQ_Frame* responseFrame = (SUPQ_Frame*)malloc(sizeof(SUPQ_Frame) + update_info_len);
    if (!responseFrame) {
        printf("Memory allocation failed\n");
        return;
    }

    responseFrame->type = SUPQ_FRAME_TYPE_UPDATE_DATA;
    responseFrame->length = htonl(update_info_len);
    memcpy(responseFrame->payload, update_info, update_info_len);

    QUIC_BUFFER Buffer = { 0 };
    Buffer.Length = sizeof(SUPQ_Frame) + update_info_len;
    Buffer.Buffer = (uint8_t*)responseFrame;

    if (QUIC_FAILED(MsQuic->StreamSend(Stream, &Buffer, 1, QUIC_SEND_FLAG_FIN, NULL))) {
        printf("StreamSend failed\n");
    }

    free(responseFrame);
}

void HandleStatusReport(SUPQ_Frame* frame) {
    printf("Handling status report\n");
    // Process the status report
    SUPQ_StatusReportPayload* statusReport = (SUPQ_StatusReportPayload*)frame->payload;
    uint32_t update_id = ntohl(statusReport->update_id);
    uint32_t status_code = ntohl(statusReport->status_code);

    printf("Status Report - Update ID: %u, Status Code: %u\n", update_id, status_code);
}

QUIC_STATUS QUIC_API StreamCallback(HQUIC Stream, void* Context, QUIC_STREAM_EVENT* Event) {
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_RECEIVE:
            printf("Stream data received\n");
            // Handle the received data here
            SUPQ_Frame* frame = (SUPQ_Frame*)Event->RECEIVE.Buffers->Buffer;
            switch (frame->type) {
                case SUPQ_FRAME_TYPE_QUERY_UPDATE:
                    HandleUpdateQuery(Stream, frame);
                    break;
                case SUPQ_FRAME_TYPE_STATUS_REPORT:
                    HandleStatusReport(frame);
                    break;
                case SUPQ_FRAME_TYPE_UPDATE_DATA:
                    printf("Received update data\n");
                    // Handle update data
                    break;
                default:
                    printf("Received unknown frame type\n");
                    break;
            }
            break;
        case QUIC_STREAM_EVENT_SEND_COMPLETE:
            printf("Stream send complete\n");
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            printf("Stream peer send shutdown\n");
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
        case QUIC_STREAM_EVENT_PEER_RECEIVE_ABORTED:
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            printf("Stream shut down\n");
            MsQuic->StreamClose(Stream);
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

void StartServer(uint16_t Port) {
    QUIC_ADDR Address = { 0 };
    Address.Ipv4.sin_family = QUIC_ADDRESS_FAMILY_INET;
    Address.Ipv4.sin_port = htons(Port);
    Address.Ipv4.sin_addr.s_addr = INADDR_ANY;

    QUIC_BUFFER AlpnBuffers[] = { { sizeof("supq") - 1, (uint8_t*)"supq" } };

    printf("Opening listener...\n");
    if (QUIC_FAILED(MsQuic->ListenerOpen(Registration, ListenerCallback, NULL, &Listener))) {
        printf("ListenerOpen failed\n");
        exit(1);
    }

    printf("ListenerOpen successful\n");

    printf("Starting listener...\n");
    QUIC_STATUS Status = MsQuic->ListenerStart(Listener, AlpnBuffers, ARRAYSIZE(AlpnBuffers), &Address);
    if (QUIC_FAILED(Status)) {
        printf("ListenerStart failed with error code: 0x%x\n", Status);
        exit(1);
    }

    printf("ListenerStart successful\n");
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    uint16_t Port = (uint16_t)atoi(argv[1]);

    InitializeMsQuic();

    StartServer(Port);

    // Keep the main thread running to handle events
    while (1) {
    }

    CleanupMsQuic();

    return 0;
}

