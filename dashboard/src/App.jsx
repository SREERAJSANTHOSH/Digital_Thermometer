import React from 'react';
import './index.css';
import { useTelemetry } from './hooks/useTelemetry';
import TelemetryPanel from './components/TelemetryPanel';
import Sparkline from './components/Sparkline';
import StatusBadge from './components/StatusBadge';
import ETAPanel from './components/ETAPanel';
import { Cpu } from 'lucide-react';

function App() {
  const telemetry = useTelemetry();

  return (
    <div className="dashboard-container">
      <div className="title-bar">
        <div>
          <h1>Thermal<span className="text-gradient">Core</span></h1>
          <p style={{ color: 'var(--text-muted)', marginTop: '4px' }}>8051 Digital Thermometer Telemetry</p>
        </div>
        <div style={{ 
          display: 'flex', alignItems: 'center', gap: '8px', 
          background: 'var(--glass-bg)', padding: '8px 16px', borderRadius: '20px',
          border: '1px solid var(--glass-border)'
        }}>
          <div style={{ width: '8px', height: '8px', borderRadius: '50%', background: 'var(--accent-green)', boxShadow: '0 0 10px var(--accent-green)' }}></div>
          <span style={{ fontSize: '0.9rem', fontWeight: '500' }}>System Online</span>
          <Cpu size={16} color="var(--text-muted)" style={{ marginLeft: '8px' }} />
        </div>
      </div>

      <TelemetryPanel temperature={telemetry.temperature} forecast={telemetry.forecast} />
      <Sparkline history={telemetry.history} />
      
      <StatusBadge quality={telemetry.quality} trend={telemetry.trend} />
      <ETAPanel eta={telemetry.eta} />
    </div>
  );
}

export default App;
