#!/bin/bash

# Generate a self-signed certificate and private key
openssl req -x509 -newkey rsa:2048 -keyout server_key.pem -out server_cert.pem -days 365 -nodes -subj "/CN=localhost"
openssl req -x509 -newkey rsa:2048 -keyout client_key.pem -out client_cert.pem -days 365 -nodes -subj "/CN=localhost"

echo "Certificates generated: server_cert.pem, server_key.pem, client_cert.pem, client_key.pem"

