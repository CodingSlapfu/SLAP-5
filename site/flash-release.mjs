/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright (C) 2026 Ziao */
import { checkUfw, logicalImage, productOf } from './protocol/fm1pkg.js';

export function validateRelease(release) {
  const { version, product, sha256, bytes, sources } = release ?? {};
  if (typeof version !== 'string' || !/^\d{1,2}\.\d{1,2}\.\d{1,2}$/.test(version) ||
      product !== 'FM-1_75' + version.split('.').map(v => v.padStart(2, '0')).join('') ||
      typeof sha256 !== 'string' || !/^[a-f0-9]{64}$/.test(sha256) ||
      !Number.isInteger(bytes) || bytes <= 0x93414 || bytes > 0x98234 ||
      !Array.isArray(sources) || !sources.length ||
      sources.some(v => typeof v !== 'string' || !/^FM-1_(015|75[0-9]{6})$/.test(v)))
    throw new Error('The release manifest is invalid.');
  return { version, product, sha256, bytes, sources, file: `SLAP-5-${version}.fwsc` };
}

export async function verifyPackage(release, pkg) {
  if (pkg.length !== release.bytes) throw new Error('Package size differs from the release manifest.');
  const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', pkg));
  if (Array.from(digest, byte => byte.toString(16).padStart(2, '0')).join('') !== release.sha256)
    throw new Error('Package SHA-256 verification failed.');
  if (productOf(pkg) !== release.product) throw new Error('Unexpected package identity.');
  const image = logicalImage(pkg);
  checkUfw(image, { slap5: true });
  return image;
}
