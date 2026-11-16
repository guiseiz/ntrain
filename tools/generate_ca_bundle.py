"""Build Espressif's compact bundle from our existing Mozilla trust roots."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from cryptography import x509
from cryptography.hazmat.primitives import serialization

ROOT=Path(__file__).resolve().parents[1]

def google_roots(pem):
    # The core's compact verifier follows Google's cross-signed R4 certificate
    # beyond our trusted GTS root to a retired GlobalSign root. Full certificates
    # let mbedTLS terminate at the existing trusted GTS root instead. Keep this
    # small subset identical to the Mozilla source, never add server certificates.
    roots=x509.load_pem_x509_certificates(pem)
    selected=[c for c in roots if any(a.value.startswith('GTS Root R')
              for a in c.subject.get_attributes_for_oid(x509.NameOID.COMMON_NAME))]
    if not selected:raise ValueError('No Google trust roots in source bundle')
    return b''.join(c.public_bytes(serialization.Encoding.PEM) for c in selected)

def verify_bundle(data,pem):
    roots=x509.load_pem_x509_certificates(pem)
    expected=sorted((c.subject.public_bytes(),c.public_key().public_bytes(
        serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo)) for c in roots)
    count,=struct.unpack_from('>H',data);offset=2;actual=[]
    for _ in range(count):
        name_len,key_len=struct.unpack_from('>HH',data,offset);offset+=4
        name=data[offset:offset+name_len];offset+=name_len
        key=data[offset:offset+key_len];offset+=key_len
        actual.append((name,key))
    if offset!=len(data) or actual!=expected:
        raise ValueError('Generated bundle does not exactly match the trusted PEM roots')
    return count

def main():
    with tempfile.TemporaryDirectory(prefix='ntrain-ca-') as folder:
        subprocess.run([sys.executable,str(ROOT/'tools/vendor/gen_crt_bundle.py'),
                        '--input',str(ROOT/'assets/ca-bundle.pem')],cwd=folder,check=True)
        data=(Path(folder)/'x509_crt_bundle').read_bytes()
    count=verify_bundle(data,(ROOT/'assets/ca-bundle.pem').read_bytes())
    (ROOT/'assets/ca-bundle.bin').write_bytes(data)
    (ROOT/'assets/google-roots.pem').write_bytes(google_roots((ROOT/'assets/ca-bundle.pem').read_bytes()))
    print(f'Verified {count} unchanged trust anchors; {len(data)} bytes in flash.')

if __name__=='__main__':main()
