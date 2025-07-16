const { exec } = require('child_process');
const fs = require('fs');

function readBarcodeFromImage(imagePath) {
  return new Promise((resolve, reject) => {
    exec(`zbarimg --quiet --nodbus --oneshot --raw ${imagePath}`, (error, stdout, stderr) => {
      const barcode = stdout.trim();

    if (error) {
      // If no barcode is found, zbarimg exits with code 4 and no stderr
      if (error.code === 4 && !barcode) {
        return reject(new Error('No barcode found in image'));
      }

      // Other errors are unexpected
      return reject(new Error(`Barcode read error: ${stderr.trim() || error.message}`));
    }

    if (!barcode) {
      return reject(new Error('No barcode found'));
    }

    resolve(barcode);
      });
  });
}

module.exports = { readBarcodeFromImage };