const express = require('express');
const axios = require('axios');
const dotenv = require('dotenv');
dotenv.config();

const app = express();
const PORT = 5000;
const OPENAI_API_KEY = process.env.OPENAI_API_KEY;

app.get('/tts', async (req, res) => {
    console.log('Neue Request!');
    console.log(req.query);
    const text = req.query.text;
    const voice = req.query.voice || 'nova';

    if (!text || !OPENAI_API_KEY) {
        return res.status(400).send('Missing "text" query or API key.');
    }

    try {
        const response = await axios.post(
            'https://api.openai.com/v1/audio/speech',
            {
                model: 'tts-1',
                input: text,
                voice: voice,
                response_format: 'mp3'
            },
            {
                responseType: 'stream',
                headers: {
                    Authorization: `Bearer ${OPENAI_API_KEY}`,
                    'Content-Type': 'application/json'
                }
            }
        );

        res.setHeader('Content-Type', 'audio/mpeg');
        response.data.pipe(res);
    } catch (error) {
        console.error('TTS request failed:', error.response?.data || error.message);
        res.status(500).send('TTS proxy error');
    }
});

app.listen(PORT, () => {
    console.log(`TTS proxy listening on port ${PORT}`);
});
