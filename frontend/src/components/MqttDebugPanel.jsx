import React, { useState, useEffect } from 'react';
import { useMqttStore } from '../hooks/useMqtt';

export function MqttDebugPanel() {
  const { connected, stations } = useMqttStore();
  const [isVisible, setIsVisible] = useState(false);

  useEffect(() => {
    const handleKeyDown = (e) => {
      if (e.altKey && e.shiftKey && e.key.toLowerCase() === 'd') {
        e.preventDefault();
        setIsVisible(prev => !prev);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  if (!isVisible) return null;

  return (
    <div className="fixed bottom-4 right-4 z-50 bg-slate-900 border border-slate-700 shadow-xl rounded-2xl p-4 w-96 text-xs font-mono text-slate-300">
      <div className="flex justify-between items-center mb-3 pb-2 border-b border-slate-800">
        <div className="font-bold text-white uppercase tracking-wider flex items-center gap-2">
          MQTT Debug
          {connected ? (
            <span className="bg-brand-500/20 text-brand-400 px-2 py-0.5 rounded text-[10px]">Connected</span>
          ) : (
            <span className="bg-rose-500/20 text-rose-400 px-2 py-0.5 rounded text-[10px]">Disconnected</span>
          )}
        </div>
        <button onClick={() => setIsVisible(false)} className="text-slate-500 hover:text-white">✕</button>
      </div>
      
      <div className="space-y-2">
        <div className="grid grid-cols-2 gap-1 text-[10px] text-slate-500 font-bold uppercase mb-1">
          <div>Station ID</div>
          <div className="text-right">KWh / Power</div>
        </div>
        {Object.entries(stations).length === 0 && (
          <div className="text-slate-500 italic py-2 text-center">No stations connected</div>
        )}
        {Object.entries(stations).map(([id, data]) => (
          <div key={id} className="grid grid-cols-2 gap-1 items-center bg-slate-800/50 rounded py-1 px-2">
            <div className="flex items-center gap-2 text-white">
              <div className={`w-1.5 h-1.5 rounded-full ${data.isOnline ? 'bg-brand-400' : 'bg-rose-400'}`}></div>
              {id}
            </div>
            <div className="text-right text-brand-300">
              {data.current_kwh?.toFixed(2)} / {data.current_power?.toFixed(2)} W
            </div>
          </div>
        ))}
      </div>
      
      <div className="mt-4 pt-3 border-t border-slate-800 flex gap-2">
        <button 
          onClick={() => window.__VINCODER_MQTT__?.ping('1')}
          className="flex-1 bg-slate-800 hover:bg-slate-700 text-slate-300 py-1.5 rounded uppercase tracking-wider text-[10px] font-bold transition-colors"
        >
          Ping MAIN
        </button>
      </div>
    </div>
  );
}
