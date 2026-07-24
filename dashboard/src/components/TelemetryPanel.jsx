import React from 'react';
import { Thermometer, Cloud } from 'lucide-react';

const TelemetryPanel = ({ temperature, forecast }) => {
  return (
    <div className="glass-panel col-span-4 fade-enter">
      <div className="flex-between">
        <div className="label">Current Temp</div>
        <Thermometer size={20} color="var(--accent-cyan)" />
      </div>
      <div style={{ marginTop: '1.5rem', marginBottom: '1.5rem' }}>
        <span className="value-large text-gradient">{temperature.toFixed(1)}</span>
        <span className="unit">°C</span>
      </div>
      <div className="flex-between" style={{ borderTop: '1px solid var(--glass-border)', paddingTop: '1rem' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Cloud size={16} color="var(--text-muted)" />
          <span style={{ fontSize: '0.9rem', color: 'var(--text-muted)' }}>Forecast</span>
        </div>
        <div style={{ fontWeight: '600', color: 'var(--text-main)' }}>{forecast}</div>
      </div>
    </div>
  );
};

export default TelemetryPanel;
