import React, { useRef, useState, useEffect } from "react";

const CameraCapture = () => {
  const videoRef = useRef(null);
  const canvasRef = useRef(null);
  const [stream, setStream] = useState(null);
  const [capturedImage, setCapturedImage] = useState(null);
  const [errorMessage, setErrorMessage] = useState("");

  useEffect(() => {
    startCamera();

    return () => {
      stopCamera();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const startCamera = () => {
    if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
      navigator.mediaDevices
        .getUserMedia({ video: true })
        .then((newStream) => {
          if (videoRef.current) {
            videoRef.current.srcObject = newStream;
          }
          setStream(newStream);
        })
        .catch((err) => {
          console.error("Error accessing camera:", err);
          setErrorMessage("Kamera-Zugriff verweigert oder nicht möglich.");
        });
    } else {
      console.error("getUserMedia wird nicht unterstützt.");
      setErrorMessage(
        "Diese Funktion wird von deinem Browser nicht unterstützt."
      );
    }
  };

  const stopCamera = () => {
    if (stream) {
      stream.getTracks().forEach((track) => track.stop());
      setStream(null);
    }
  };

  const handleCapture = () => {
    const video = videoRef.current;
    const canvas = canvasRef.current;
    if (video && canvas) {
      canvas.width = video.videoWidth;
      canvas.height = video.videoHeight;
      const ctx = canvas.getContext("2d");
      ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
      const dataUrl = canvas.toDataURL("image/png");
      setCapturedImage(dataUrl);
      stopCamera();
    }
  };

  const handleRetake = () => {
    setCapturedImage(null);
    startCamera();
  };

  const handleSend = async () => {
    try {
      // Data-URL Prefix entfernen
      const base64 = capturedImage.split(",")[1];

      const response = await fetch("/image", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          image: base64,
        }),
      });
      console.log(response);
      if (!response.ok) {
        const errorData = await response.json();
        console.error("Error sending image:", errorData);
        alert("Fehler beim Senden des Bildes!");
        return;
      }

      const result = await response.json();
      console.log("Server response:", result);
      alert("Bild erfolgreich gesendet!");
    } catch (err) {
      console.error("Error sending image:", err);
      alert("Fehler beim Senden des Bildes!");
    }
  };

  return (
    <div style={{ textAlign: "center" }}>
      {errorMessage ? (
        <p style={{ color: "red" }}>{errorMessage}</p>
      ) : !capturedImage ? (
        <>
          <video
            ref={videoRef}
            autoPlay
            playsInline
            style={{ width: "100%", maxWidth: "400px" }}
          />
          <br />
          <button onClick={handleCapture}>Aufnehmen</button>
        </>
      ) : (
        <>
          <img
            src={capturedImage}
            alt="Captured"
            style={{ width: "100%", maxWidth: "400px" }}
          />
          <br />
          <button onClick={handleSend}>Senden</button>
          <button onClick={handleRetake}>Wiederholen</button>
        </>
      )}
      {/* Unsichtbares Canvas für das Foto */}
      <canvas ref={canvasRef} style={{ display: "none" }} />
    </div>
  );
};

export default CameraCapture;
