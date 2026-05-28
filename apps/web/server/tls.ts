import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tlsCertPath, tlsKeyPath } from "./config";
import { localIPv4Addresses } from "./network";

export function loadTlsOptions(): Bun.TLSOptions {
  ensureLocalCertificate();

  return {
    cert: Bun.file(tlsCertPath),
    key: Bun.file(tlsKeyPath)
  };
}

function ensureLocalCertificate() {
  if (existsSync(tlsCertPath) && existsSync(tlsKeyPath)) {
    return;
  }

  const certDir = dirname(tlsCertPath);
  mkdirSync(certDir, { recursive: true });
  mkdirSync(dirname(tlsKeyPath), { recursive: true });

  const configPath = join(certDir, "peek-local-openssl.cnf");
  writeFileSync(configPath, opensslConfig(), "utf8");

  const result = spawnSync(
    "openssl",
    [
      "req",
      "-x509",
      "-newkey",
      "rsa:2048",
      "-nodes",
      "-sha256",
      "-days",
      "825",
      "-keyout",
      tlsKeyPath,
      "-out",
      tlsCertPath,
      "-config",
      configPath,
      "-extensions",
      "v3_req"
    ],
    {
      encoding: "utf8",
      stdio: "pipe"
    }
  );

  if (result.status !== 0) {
    throw new Error(
      `Failed to generate local HTTPS certificate with openssl.\n${result.stderr || result.stdout}`
    );
  }
}

function opensslConfig() {
  const ipAddresses = ["127.0.0.1", ...localIPv4Addresses()];
  const altNames = [
    "DNS.1 = localhost",
    ...ipAddresses.map((address, index) => `IP.${index + 1} = ${address}`)
  ];

  return [
    "[req]",
    "default_bits = 2048",
    "prompt = no",
    "distinguished_name = dn",
    "x509_extensions = v3_req",
    "",
    "[dn]",
    "CN = Peek Local",
    "",
    "[v3_req]",
    "basicConstraints = critical, CA:false",
    "keyUsage = critical, digitalSignature, keyEncipherment",
    "extendedKeyUsage = serverAuth",
    "subjectAltName = @alt_names",
    "",
    "[alt_names]",
    ...altNames,
    ""
  ].join("\n");
}
