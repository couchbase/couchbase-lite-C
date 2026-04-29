#!/usr/bin/env bash
#
# Regenerates the Root CA / Intermediate CA / Leaf certificate chain embedded
# as PEMs
#
# A chain of CA certs:
#     int3, int2, int1, root
# where root is self-signed CA, int1 is signed by root, int2 is signed by int1, int3 is signed by int2.
# The CNs of them are, respectively, : "Intermediate3 CA", "Intermediate2 CA", "Intermediate1 CA", "My Root CA".

# A leaf cert with its private key
#     leaf.key
#     leaf.pem
# where leaf.pem is signed by int1
#
# Usage:
#   ./gen_cert_chain.sh [years]
#
# `years` is the validity period for the leaf and intermediate (default 20).
# The root CA gets years*2 so it always outlives what it signs.

set -euo pipefail

YEARS="${1:-20}"
LEAF_DAYS=$(( YEARS * 365 ))
INT_DAYS=$(( YEARS * 365 ))
ROOT_DAYS=$(( YEARS * 2 * 365 ))

setup_work_dir() {
    local WORK="${1:-GEN-CERT-CHAIN-WORK}"

    if [ ! -d "$WORK" ]; then
        echo "Created directory: $WORK" >&2
        mkdir "$WORK"
    else
        echo "Directory $WORK already exists. Searching for available numbered variant..." >&2
        local n=1
        while [ -d "${WORK}-${n}" ]; do
            echo "  ${WORK}-${n} exists, trying next..." >&2
            n=$((n + 1))
        done
        WORK="${WORK}-${n}"
        mkdir "$WORK"
        echo "Created directory: $WORK" >&2
    fi

    echo "$WORK"  # ← only this goes to stdout, captured by $()
}

WORK=$(setup_work_dir)

pushd $WORK
trap popd EXIT

# ---- Root CA (self-signed) ----
# -------------------------------
openssl genrsa -out root.key 2048 2>/dev/null
openssl req -x509 -new -key root.key -sha256 -days "$ROOT_DAYS" \
    -subj "/CN=My Root CA" \
    -extensions v3_ca \
    -config <(cat <<'CFG'
[req]
distinguished_name = dn
[dn]
[v3_ca]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer
basicConstraints = critical, CA:TRUE
keyUsage = critical, keyCertSign, cRLSign
CFG
) -out root.pem

# ---- extension of intermediate certs
cat > int.ext <<'CFG'
basicConstraints = critical, CA:TRUE
keyUsage = critical, keyCertSign, cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer
CFG

# ---- Intermediate CA, int1, (signed by Root) ----
openssl genrsa -out int1.key 2048 2>/dev/null
openssl req -new -key int1.key -subj "/CN=Intermediate1 CA" -out int1.csr
openssl x509 -req -in int1.csr -CA root.pem -CAkey root.key -CAcreateserial \
    -out int1.pem -days "$INT_DAYS" -sha256 -extfile int.ext 2>/dev/null


# ---- Intermediate CA, int2, (signed by int1) ----
openssl genrsa -out int2.key 2048 2>/dev/null
openssl req -new -key int2.key -subj "/CN=Intermediate2 CA" -out int2.csr
openssl x509 -req -in int2.csr -CA int1.pem -CAkey int1.key -CAcreateserial \
    -out int2.pem -days "$INT_DAYS" -sha256 -extfile int.ext 2>/dev/null


# ---- Intermediate CA, int3, (signed by int2) ----
openssl genrsa -out int3.key 2048 2>/dev/null
openssl req -new -key int3.key -subj "/CN=Intermediate3 CA" -out int3.csr
openssl x509 -req -in int3.csr -CA int2.pem -CAkey int2.key -CAcreateserial \
    -out int3.pem -days "$INT_DAYS" -sha256 -extfile int.ext 2>/dev/null


# ---- Leaf (signed by Intermediate) ----
# ---------------------------------------
openssl genrsa -out leaf.key 2048 2>/dev/null
openssl req -new -key leaf.key -subj "/CN=Leaf Cert" -out leaf.csr

cat > leaf.ext <<'CFG'
basicConstraints = CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth, clientAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid,issuer
CFG

# ---- Sign the Leaf by int1 -------

openssl x509 -req -in leaf.csr -CA int1.pem -CAkey int1.key -CAcreateserial \
    -out leaf.pem -days "$LEAF_DAYS" -sha256 -extfile leaf.ext 2>/dev/null

# echo "==================== leaf key ===================="
# cat leaf.key
# echo "==================== leaf (cert1) ===================="
# cat leaf.pem

# echo "==================== inter3  ===================="
# cat int3.pem

# echo "==================== inter2  ===================="
# cat int2.pem

# echo "==================== inter1 ===================="
# cat int1.pem
# echo "==================== root ===================="
# cat root.pem

echo ""
echo "Expirations:"
echo -n "  leaf:         "; openssl x509 -in leaf.pem -noout -enddate

echo -n "  inter3:       "; openssl x509 -in int3.pem  -noout -enddate
echo -n "  inter2:       "; openssl x509 -in int2.pem  -noout -enddate
echo -n "  inter1:       "; openssl x509 -in int1.pem  -noout -enddate
echo -n "  root:         "; openssl x509 -in root.pem -noout -enddate
