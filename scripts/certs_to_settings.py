#! /usr/bin/env python

import requests
import json
import sys

def get_certs(root_ca_file, priv_key_file, client_cert_file):
    
    return {
             'caCert': open(root_ca_file, 'r').read().replace('\r', ''),
             'privateKey': open(priv_key_file, 'r').read().replace('\r', ''),
             'clientCert': open(client_cert_file, 'r').read().replace('\r', '')
             }

def save_certs_to_settings(filename, ca_cert, priv_key, client_cert):

    with open(filename, 'r+') as f:
        settings_data = json.load(f)

        settings_data['SSL_certificates']['root_CA'] = ca_cert
        settings_data['SSL_certificates']['private_key'] = priv_key
        settings_data['SSL_certificates']['certificate_pem'] = client_cert

        f.seek(0)
        json.dump(settings_data, f, indent=4)

def main():
    if len(sys.argv) != 5:
        print "Usage: certs_to_settings.py <root CA> <private key> <client cert> <settings file>"
        return

    root_ca, priv_key, client_cert, settings_file = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]

    certs = get_certs(root_ca, priv_key, client_cert)

    save_certs_to_settings(settings_file, certs['caCert'], certs['privateKey'], certs['clientCert'])
    

if (__name__ == "__main__"):
    main()
