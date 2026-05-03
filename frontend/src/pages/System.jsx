import React, { useState, useEffect } from 'react';
import { useAuth } from '../components/AuthContext';
import { api } from '../api';
import { Copy, Users, Link as LinkIcon, Ban, CheckCircle2, MapPin } from 'lucide-react';

export default function SystemPage() {
  const { user } = useAuth();
  const [users, setUsers] = useState([]);
  const [invites, setInvites] = useState([]);
  const [stats, setStats] = useState([]);
  const [allStations, setAllStations] = useState([]);
  
  // Invite form
  const [targetRole, setTargetRole] = useState(user.role === 'super_admin' ? 'owner' : 'employee');
  const [selectedStations, setSelectedStations] = useState([]);
  const [generating, setGenerating] = useState(false);
  const [newInviteToken, setNewInviteToken] = useState('');

  // Fetch data
  const fetchData = async () => {
    try {
      const [u, i, s, st] = await Promise.all([
        api.get('/users'),
        api.get('/invites'),
        api.get('/free-charging/stats'),
        api.get('/stations')
      ]);
      setUsers(u);
      setInvites(i);
      setStats(s);
      setAllStations(st);
    } catch (e) {
      console.error(e);
    }
  };

  useEffect(() => {
    fetchData();
  }, []);

  const handleGenerateInvite = async (e) => {
    e.preventDefault();
    setGenerating(true);
    setNewInviteToken('');
    try {
      const payload = { target_role: targetRole };
      if (selectedStations.length > 0) {
        payload.station_ids = selectedStations.map(Number);
      }
      const data = await api.post('/invites', payload);
      const url = `${window.location.origin}/register?token=${data.token}`;
      setNewInviteToken(url);
      fetchData(); // Refresh invites list
    } catch (e) {
      alert(e.message);
    } finally {
      setGenerating(false);
    }
  };

  const toggleUserLock = async (u) => {
    try {
      await api.patch(`/users/${u.id}/toggle-active`, { is_active: !u.is_active });
      fetchData();
    } catch (e) {
      alert(e.message);
    }
  };

  const toggleStationSelection = (id) => {
    if (selectedStations.includes(id)) {
      setSelectedStations(selectedStations.filter(s => s !== id));
    } else {
      setSelectedStations([...selectedStations, id]);
    }
  };

  // Only allowed to create invites for certain roles
  const availableRoles = user.role === 'super_admin' ? ['owner', 'employee'] : ['employee'];

  return (
    <div className="max-w-6xl mx-auto space-y-8">
      <div>
        <h1 className="text-2xl font-bold text-brand-dark">Настройки</h1>
        <p className="text-slate-500 text-sm">RBAC, Инвайты и статистика</p>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
        
        {/* Left Column: Generate Invites */}
        <div className="lg:col-span-1 space-y-6">
          <div className="bg-white p-6 rounded-3xl shadow-sm border border-slate-100">
            <h2 className="text-sm font-bold text-slate-800 uppercase tracking-wider mb-6 flex items-center gap-2">
              <LinkIcon size={16} className="text-brand" /> Генерация инвайта
            </h2>
            
            <form onSubmit={handleGenerateInvite} className="space-y-4">
              <div>
                <label className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 block">Роль</label>
                <select 
                  value={targetRole} 
                  onChange={(e) => setTargetRole(e.target.value)}
                  className="w-full px-4 py-2.5 bg-slate-50 border-2 border-slate-100 rounded-xl text-sm font-bold focus:border-brand-300"
                >
                  {availableRoles.map(r => <option key={r} value={r}>{r}</option>)}
                </select>
              </div>

              {/* Station Selection for employees OR if owner wants to limit owner */}
              {(targetRole === 'employee' || user.role === 'owner') && (
                <div>
                  <label className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 block">Доступные станции</label>
                  <div className="max-h-[200px] overflow-y-auto space-y-2 pr-2">
                    {allStations?.map(station => (
                      <label key={station.id} className="flex items-center justify-between p-3 bg-slate-50 rounded-xl cursor-pointer hover:bg-slate-100 border border-transparent hover:border-slate-200">
                        <div className="text-sm font-bold text-slate-700 flex items-center gap-2">
                          <MapPin size={14} className="text-brand-500"/>
                          {station.name}
                        </div>
                        <input 
                          type="checkbox" 
                          checked={selectedStations.includes(station.id)}
                          onChange={() => toggleStationSelection(station.id)}
                          className="w-4 h-4 text-brand rounded focus:ring-brand accent-brand border-slate-300" 
                        />
                      </label>
                    ))}
                  </div>
                </div>
              )}

              <button 
                type="submit" disabled={generating}
                className="w-full py-3 bg-brand text-white font-bold rounded-xl text-xs uppercase tracking-wider shadow-md shadow-brand-200 hover:bg-brand-dark transition-all disabled:opacity-50"
              >
                {generating ? 'Генерация...' : 'Создать ссылку'}
              </button>
            </form>

            {newInviteToken && (
              <div className="mt-4 p-4 bg-brand-50 border-2 border-brand-100 rounded-xl relative">
                <div className="text-[10px] font-bold text-brand-600 uppercase tracking-wider mb-1">Ссылка скопирована</div>
                <div className="text-xs font-medium text-brand-900 break-all">{newInviteToken}</div>
                <button 
                  onClick={() => navigator.clipboard.writeText(newInviteToken)}
                  className="absolute top-4 right-4 text-brand-600 hover:text-brand-800"
                >
                  <Copy size={16}/>
                </button>
              </div>
            )}
          </div>

          <div className="bg-white p-6 rounded-3xl shadow-sm border border-slate-100">
            <h2 className="text-sm font-bold text-slate-800 uppercase tracking-wider mb-4">Бесплатный режим</h2>
            <div className="space-y-4">
              {stats.length === 0 ? (
                <div className="text-sm text-slate-500 font-medium text-center py-4">Нет данных</div>
              ) : (
                stats.map(s => (
                  <div key={s.user_id} className="p-4 bg-slate-50 rounded-xl flex items-center justify-between">
                    <div>
                      <div className="font-bold text-sm text-brand-dark">{s.full_name}</div>
                      <div className="text-[10px] font-bold text-slate-400 tracking-wider uppercase mt-0.5">{s.session_count} сессий</div>
                    </div>
                    <div className="text-right">
                      <div className="font-black text-brand-500">{s.total_kwh.toFixed(1)} <span className="text-xs">кВт·ч</span></div>
                    </div>
                  </div>
                ))
              )}
            </div>
          </div>
        </div>

        {/* Right Column: Users List */}
        <div className="lg:col-span-2 space-y-6">
          <div className="bg-white p-6 rounded-3xl shadow-sm border border-slate-100 h-full">
            <div className="flex items-center justify-between mb-6">
              <h2 className="text-sm font-bold text-slate-800 uppercase tracking-wider flex items-center gap-2">
                <Users size={16} className="text-brand" /> Пользователи системы
              </h2>
              <div className="text-xs font-bold text-slate-400 bg-slate-50 px-3 py-1 rounded-full">{users.length} всего</div>
            </div>

            <div className="overflow-x-auto">
              <table className="w-full text-left border-collapse">
                <thead>
                  <tr>
                    <th className="pb-3 text-[10px] font-bold text-slate-400 uppercase tracking-wider border-b-2 border-slate-50">Пользователь</th>
                    <th className="pb-3 text-[10px] font-bold text-slate-400 uppercase tracking-wider border-b-2 border-slate-50">Роль/Доступ</th>
                    <th className="pb-3 text-[10px] font-bold text-slate-400 uppercase tracking-wider border-b-2 border-slate-50 text-right">Действия</th>
                  </tr>
                </thead>
                <tbody>
                  {users.map(u => (
                    <tr key={u.id} className={`border-b-2 border-slate-50 last:border-0 ${!u.is_active ? 'opacity-50 grayscale' : ''}`}>
                      <td className="py-4">
                        <div className="font-bold text-sm text-brand-dark">{u.full_name}</div>
                        <div className="text-xs font-medium text-slate-500">{u.email}</div>
                      </td>
                      <td className="py-4">
                        <span className={`inline-block px-2 py-1 rounded-md text-[10px] font-bold uppercase tracking-wider ${
                          u.role === 'super_admin' ? 'bg-amber-100 text-amber-700' : 
                          u.role === 'owner' ? 'bg-brand-100 text-brand-700' : 
                          'bg-slate-100 text-slate-600'
                        }`}>
                          {u.role}
                        </span>
                        {u.station_ids?.length > 0 && (
                          <div className="mt-1 flex gap-1 flex-wrap">
                            {u.station_ids.map(id => (
                              <span key={id} className="text-[9px] font-bold text-brand-500 bg-brand-50 px-1.5 py-0.5 rounded">ST-{id}</span>
                            ))}
                          </div>
                        )}
                      </td>
                      <td className="py-4 text-right">
                        {u.id !== user.id && (
                          <div className="flex items-center justify-end gap-2">
                            <button 
                              onClick={() => toggleUserLock(u)}
                              className={`p-2 rounded-xl transition-all ${
                                u.is_active ? 'bg-rose-50 text-rose-500 hover:bg-rose-100' : 'bg-green-50 text-green-600 hover:bg-green-100'
                              }`}
                              title={u.is_active ? "Заблокировать" : "Разблокировать"}
                            >
                              {u.is_active ? <Ban size={18} /> : <CheckCircle2 size={18} />}
                            </button>
                            {user.role === 'super_admin' && (
                              <button 
                                onClick={async () => {
                                  if (confirm(`Удалить пользователя ${u.full_name}?`)) {
                                    try {
                                      await api.delete(`/users/${u.id}`);
                                      fetchData();
                                    } catch (e) {
                                      alert(e.message);
                                    }
                                  }
                                }}
                                className="p-2 rounded-xl transition-all bg-red-50 text-red-600 hover:bg-red-100"
                                title="Удалить"
                              >
                                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M3 6h18"></path><path d="M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6"></path><path d="M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2"></path><line x1="10" y1="11" x2="10" y2="17"></line><line x1="14" y1="11" x2="14" y2="17"></line></svg>
                              </button>
                            )}
                          </div>
                        )}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        </div>

      </div>
    </div>
  );
}
