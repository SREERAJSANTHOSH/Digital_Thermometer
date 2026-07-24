import { useState, useEffect } from 'react';

const generateMockHistory = (count, min, max) => {
  return Array.from({ length: count }, () => 
    parseFloat((Math.random() * (max - min) + min).toFixed(1))
  );
};

export const useTelemetry = () => {
  const [data, setData] = useState({
    temperature: 28.5,
    history: generateMockHistory(16, 24.0, 32.0),
    eta: 45, // seconds
    forecast: 'Sunny',
    quality: 'High',
    trend: 'Stable'
  });

  useEffect(() => {
    const interval = setInterval(() => {
      setData(prev => {
        const newTemp = parseFloat((prev.temperature + (Math.random() - 0.5) * 1.5).toFixed(1));
        const clampedTemp = Math.min(Math.max(newTemp, 24.0), 38.0);
        
        const newHistory = [...prev.history.slice(1), clampedTemp];
        
        let trend = 'Stable';
        const diff = clampedTemp - prev.history[prev.history.length - 1];
        if (diff > 0.5) trend = 'Rising';
        else if (diff < -0.5) trend = 'Falling';

        let quality = 'High';
        if (clampedTemp > 35 || clampedTemp < 25) quality = 'Low';
        else if (clampedTemp > 32 || clampedTemp < 27) quality = 'Med';

        const newEta = Math.max(0, prev.eta - 3);

        const forecasts = ['Sunny', 'Clear', 'Humid', 'Dry'];
        const forecast = forecasts[Math.floor(Math.random() * forecasts.length)];

        return {
          temperature: clampedTemp,
          history: newHistory,
          eta: newEta === 0 ? 60 : newEta,
          forecast: prev.eta === 0 ? forecast : prev.forecast,
          quality,
          trend
        };
      });
    }, 3000);

    return () => clearInterval(interval);
  }, []);

  return data;
};
