"""Ensure the embedded trust bundle is regenerated whenever its source changes."""
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from generate_ca_bundle import verify_bundle, google_roots
from cryptography import x509
from cryptography.hazmat.primitives import serialization

def test_embedded_trust_anchors_match_the_source_pem():
    pem=(ROOT/'assets/ca-bundle.pem').read_bytes()
    count=verify_bundle((ROOT/'assets/ca-bundle.bin').read_bytes(),pem)
    assert count==pem.count(b'-----BEGIN CERTIFICATE-----')

def test_google_path_uses_only_existing_trusted_root_certificates():
    pem=(ROOT/'assets/ca-bundle.pem').read_bytes()
    generated=google_roots(pem)
    assert generated==(ROOT/'assets/google-roots.pem').read_bytes()
    trusted={c.public_bytes(serialization.Encoding.DER) for c in x509.load_pem_x509_certificates(pem)}
    roots=x509.load_pem_x509_certificates(generated)
    assert all(c.public_bytes(serialization.Encoding.DER) in trusted for c in roots)
    assert all(c.subject==c.issuer for c in roots)
    assert any(c.subject.get_attributes_for_oid(x509.NameOID.COMMON_NAME)[0].value=='GTS Root R4' for c in roots)
