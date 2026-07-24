import React from 'react';
import { Activity } from 'lucide-react';

const Sparkline = ({ history }) => {
  const min = Math.min(...history) - 1;
  const max = Math.max(...history) + 1;
  const range = max - min;
  
  const width = 100;
  const height = 40;
  
  const points = history.map((val, i) => {
    const x = (i / (history.length - 1)) * width;
    const y = height - ((val - min) / range) * height;
    return `${x},${y}`;
  }).join(' L ');

  const pathD = `M ${points}`;
  const areaD = `M ${points} L ${width},${height} L 0,${height} Z`;

  return (
    <div className="glass-panel col-span-8 fade-enter">
      <div className="flex-between">
        <div className="label">History (Last 16)</div>
        <Activity size={20} color="var(--accent-teal)" />
      </div>
      <div style={{ marginTop: '1rem', width: '100%', height: '120px' }}>
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" style={{ width: '100%', height: '100%', overflow: 'visible' }}>
          <defs>
            <linearGradient id="sparkGradient" x1="0" x2="0" y1="0" y2="1">
              <stop offset="0%" stopColor="var(--accent-teal)" stopOpacity="0.4" />
              <stop offset="100%" stopColor="var(--accent-cyan)" stopOpacity="0.0" />
            </linearGradient>
          </defs>
          <path d={areaD} fill="url(#sparkGradient)" />
          <path d={pathD} fill="none" stroke="var(--accent-teal)" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" 
            style={{ filter: 'drop-shadow(0 2px 4px rgba(0,255,200,0.3))' }}
          />
          {history.map((val, i) => {
            const x = (i / (history.length - 1)) * width;
            const y = height - ((val - min) / range) * height;
            return (
              <circle key={i} cx={x} cy={y} r="1" fill="var(--bg-color)" stroke="var(--accent-cyan)" strokeWidth="0.5" />
            );
          })}
        </svg>
      </div>
    </div>
  );
};

export default Sparkline;
