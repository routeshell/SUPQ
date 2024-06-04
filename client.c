#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "msquic.h"
#include "supq.h"

// Ensure MsQuicOpen is declared properly
extern const QUIC_API_TABLE* MsQuic;
QUIC_STATUS MsQuicOpen(const QUIC_API_TABLE** QuicApi);

#define CLIENT_CERT_FILE "client_cert.pem"
#define CLIENT_KEY_FILE  "client_key.pem"

#ifndef ARRAYSIZE
#define ARRAYSIZE(A) (sizeof(A) / sizeof((A)[0]))
#endif

const QUIC_API_TABLE* MsQuic;
HQUIC Registration;
HQUIC Configuration;
HQUIC StreamEvent;
volatile int Done = 0;

QUIC_STATUS QUIC_API StreamCallback(HQUIC Stream, void* Context, QUIC_STREAM_EVENT* Event);
QUIC_STATUS QUIC_API ConnectionCallback(HQUIC Connection, void* Context, QUIC_CONNECTION_EVENT* Event);

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

    QUIC_REGISTRATION_CONFIG RegConfig = { "SUPQClient", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
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
    CredConfig.CertificateFile->CertificateFile = CLIENT_CERT_FILE;
    CredConfig.CertificateFile->PrivateKeyFile = CLIENT_KEY_FILE;

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

QUIC_STATUS QUIC_API ConnectionCallback(HQUIC Connection, void* Context, QUIC_CONNECTION_EVENT* Event) {
    switch (Event->Type) {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            printf("Connection connected\n");
            break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            printf("Connection shut down\n");
            Done = 1;
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API StreamCallback(HQUIC Stream, void* Context, QUIC_STREAM_EVENT* Event) {
    switch (Event->Type) {
        case QUIC_STREAM_EVENT_RECEIVE:
            printf("Stream data received\n");
            // Handle the received data here
            SUPQ_Frame* frame = (SUPQ_Frame*)Event->RECEIVE.Buffers->Buffer;
            switch (frame->type) {
                case SUPQ_FRAME_TYPE_QUERY_UPDATE:
                    printf("Received update query\n");
                    // Handle update query
                    break;
                case SUPQ_FRAME_TYPE_UPDATE_DATA:
                    printf("Received update data: %s\n", frame->payload);
                    Done = 1; // Mark as done after receiving data
                    break;
                case SUPQ_FRAME_TYPE_STATUS_REPORT:
                    printf("Received status report\n");
                    // Handle status report
                    Done = 1; // Mark as done after receiving status report
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
            Done = 1; // Mark as done when the stream shuts down
            break;
        default:
            break;
    }
    return QUIC_STATUS_SUCCESS;
}

void SendStatusReport(HQUIC Stream, uint32_t update_id, uint32_t status_code) {
    SUPQ_Frame* statusFrame = (SUPQ_Frame*)malloc(sizeof(SUPQ_Frame) + sizeof(SUPQ_StatusReportPayload));
    if (!statusFrame) {
        printf("Memory allocation failed\n");
        return;
    }

    statusFrame->type = SUPQ_FRAME_TYPE_STATUS_REPORT;
    statusFrame->length = htonl(sizeof(SUPQ_StatusReportPayload));

    SUPQ_StatusReportPayload* statusPayload = (SUPQ_StatusReportPayload*)statusFrame->payload;
    statusPayload->update_id = htonl(update_id);
    statusPayload->status_code = htonl(status_code);

    QUIC_BUFFER Buffer = { 0 };
    Buffer.Length = sizeof(SUPQ_Frame) + sizeof(SUPQ_StatusReportPayload);
    Buffer.Buffer = (uint8_t*)statusFrame;

    if (QUIC_FAILED(MsQuic->StreamSend(Stream, &Buffer, 1, QUIC_SEND_FLAG_FIN, NULL))) {
        printf("StreamSend failed\n");
    }

    free(statusFrame);
}

void StartClient(const char* ServerName, uint16_t Port, int Operation) {
    HQUIC Connection = NULL;
    QUIC_ADDR Address = {0};

    printf("Opening connection...\n");
    if (QUIC_FAILED(MsQuic->ConnectionOpen(Registration, ConnectionCallback, NULL, &Connection))) {
        printf("ConnectionOpen failed\n");
        exit(1);
    }

    printf("ConnectionOpen successful\n");

    printf("Starting connection...\n");

    // Initialize the Address structure
    Address.Ipv4.sin_family = QUIC_ADDRESS_FAMILY_INET;
    Address.Ipv4.sin_port = htons(Port);
    inet_pton(AF_INET, ServerName, &Address.Ipv4.sin_addr);

    QUIC_STATUS Status = MsQuic->ConnectionStart(Connection, Configuration, QUIC_ADDRESS_FAMILY_UNSPEC, ServerName, Port);
    if (QUIC_FAILED(Status)) {
        printf("ConnectionStart failed with error code: 0x%x\n", Status);
        MsQuic->ConnectionClose(Connection); // Clean up the connection if it failed to start
        exit(1);
    }

    printf("ConnectionStart successful\n");

    // Open a stream
    HQUIC Stream = NULL;
    if (QUIC_FAILED(MsQuic->StreamOpen(Connection, QUIC_STREAM_OPEN_FLAG_NONE, StreamCallback, NULL, &Stream))) {
        printf("StreamOpen failed\n");
        MsQuic->ConnectionClose(Connection);
        exit(1);
    }

    printf("StreamOpen successful\n");

    // Perform the operation
    // For example, send a message or handle other client logic based on the operation parameter

    if (Operation == 0) { // Query update
        const char* version = "1.0.0";
        size_t version_length = strlen(version);
        size_t payload_length = sizeof(SUPQ_QueryUpdatePayload) + version_length;
        size_t frame_length = sizeof(SUPQ_Frame) + payload_length;

        SUPQ_Frame* queryFrame = (SUPQ_Frame*)malloc(frame_length);
        if (!queryFrame) {
            printf("Memory allocation failed\n");
            MsQuic->StreamClose(Stream);
            MsQuic->ConnectionClose(Connection);
            exit(1);
        }

        queryFrame->type = SUPQ_FRAME_TYPE_QUERY_UPDATE;
        queryFrame->length = htonl(payload_length);

        SUPQ_QueryUpdatePayload* queryPayload = (SUPQ_QueryUpdatePayload*)queryFrame->payload;
        queryPayload->software_version_length = htonl(version_length);
        queryPayload->update_type = SUPQ_UPDATE_TYPE_SECURITY_PATCH;
        memcpy(queryPayload->software_version, version, version_length);

        QUIC_BUFFER Buffer = { 0 };
        Buffer.Length = frame_length;
        Buffer.Buffer = (uint8_t*)queryFrame;

        if (QUIC_FAILED(MsQuic->StreamSend(Stream, &Buffer, 1, QUIC_SEND_FLAG_FIN, NULL))) {
            printf("StreamSend failed\n");
        }

        free(queryFrame);
    } else if (Operation == 1) { // Download update
        // Implement download logic here
    } else if (Operation == 2) { // Report status
        SendStatusReport(Stream, 1, SUPQ_STATUS_SUCCESS);
    }

    // Wait until done
    while (!Done) {
    MsQuic->StreamClose(Stream);
    MsQuic->ConnectionClose(Connection);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <server> <port> <operation>\n", argv[0]);
        return 1;
    }

    const char* ServerName = argv[1];
    uint16_t Port = (uint16_t)atoi(argv[2]);
    int Operation = atoi(argv[3]);

    InitializeMsQuic();

    StartClient(ServerName, Port, Operation);

    CleanupMsQuic();

    return 0;
}

