# SUPQ (System Update Protocol over QUIC)

## Overview

This application is designed to handle the distribution and management of system updates across devices efficiently and securely using the QUIC protocol.


## Prerequisites

- GCC (GNU Compiler Collection)
- OpenSSL (for generating certificates)
- MsQuic library

## Generating Certificates

To generate the necessary self-signed certificates for both the server and client, use the following commands:

```bash
openssl req -x509 -newkey rsa:2048 -keyout server_key.pem -out server_cert.pem -days 365 -nodes -subj "/CN=localhost"
openssl req -x509 -newkey rsa:2048 -keyout client_key.pem -out client_cert.pem -days 365 -nodes -subj "/CN=localhost"

echo "Certificates generated: server_cert.pem, server_key.pem, client_cert.pem, client_key.pem"
```

## Building the Project

A Makefile is provided to simplify the build process. The Makefile will compile both the server and client applications.

1. Ensure you have the necessary dependencies installed.
2. Ensure your directory structure includes:
   - `client.c`
   - `server.c`
   - `supq.h`
   - `Makefile`
   - `server_cert.pem`, `server_key.pem`, `client_cert.pem`, `client_key.pem` (after running the certificate generation script)

To build the project, run the following command in the project directory:

```bash
make
```

This command will compile the server and client applications.

## Running the Server and Client

### Running the Server

To start the server, run the following command (replace `5544` with your desired port number):

```bash
./server 5544
```

### Running the Client

To run the client and perform different operations, use the following commands:

#### Query Updates

To query updates from the server:

```bash
./client localhost 5544 0
```


https://github.com/routeshell/SUPQ