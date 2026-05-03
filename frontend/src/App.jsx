import React, { useState, useMemo, useEffect } from 'react';
import { Routes, Route, Navigate, useNavigate } from 'react-router-dom';
import { api } from './api';
import { useAuth } from './components/AuthContext';
import Login from './pages/Login';
import Register from './pages/Register';
import SystemPage from './pages/System';
import {
  LayoutDashboard,
  Wallet,
  Activity,
  Settings,
  History,
  Zap,
  Cpu,
  MapPin,
  Menu,
  X,
  CreditCard,
  User,
  Power,
  RotateCcw,
  Play,
  Square,
  Wifi,
  Clock
} from 'lucide-react';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  ComposedChart,
  Bar
} from 'recharts';
import { useMqttStore, useMqttStation } from './hooks/useMqtt';
import { MqttDebugPanel } from './components/MqttDebugPanel';

function Sidebar({ activeTab, setActiveTab, mobileOpen, setMobileOpen, user, logout }) {
  const navItems = [
    { id: 'economy', label: 'Экономика', icon: LayoutDashboard, roles: ['super_admin', 'owner'] },
    { id: 'transactions', label: 'Транзакции', icon: History, roles: ['super_admin', 'owner'] },
    { id: 'assets', label: 'Колонки', icon: Zap, roles: ['super_admin', 'owner', 'employee'] },
    { id: 'system', label: 'Настройки', icon: Settings, roles: ['super_admin', 'owner'] }
  ].filter(item => item.roles.includes(user?.role));

  const content = (
    <div className="flex flex-col h-full bg-white">
      <div className="p-6">
        <div className="flex items-center gap-3">
          <div>
            <svg width="121" height="40" viewBox="0 0 121 40" fill="none" xmlns="http://www.w3.org/2000/svg" className="h-7 w-auto">
              <g clipPath="url(#clip0_2028_1728)">
                <path d="M0 -1.90735e-06C0 0.235198 0.956822 4.19083 7.35689 29.1862C7.8672 31.1747 8.59013 33.4411 8.97286 34.2109C10.2486 36.8622 12.9277 38.9363 15.9896 39.6632C17.9245 40.1123 29.1512 40.1123 30.8947 39.6632C33.4675 39.0004 35.4237 37.5037 37.1247 34.9592C37.5925 34.2536 39.74 30.4049 41.8875 26.4065C44.0351 22.4081 46.3314 18.1318 46.9906 16.8916C50.7541 9.96392 55.9209 0.128289 55.9209 -1.90735e-06H37.3729L30.8309 12.3373C27.2588 19.0512 24.282 24.5035 24.197 24.418C24.0481 24.247 23.4528 22.0019 20.7311 11.3324C19.6042 6.9277 18.4985 2.56581 18.2434 1.6464L17.8181 -1.90735e-06H8.90907C3.08309 -1.90735e-06 0 -1.90735e-06 0 -1.90735e-06ZM15.4792 5.2813C15.7344 6.20072 17.0314 11.3324 18.3922 16.6778C21.9218 30.4904 21.773 29.9345 22.3046 30.4904C22.8999 31.1105 24.1119 31.1747 24.8136 30.6187C25.2601 30.2552 26.5784 27.8391 36.4655 9.23694L39.4423 3.65629L44.6091 3.6349C48.3089 3.6349 49.7547 3.69905 49.7547 3.87011C49.7547 4.06254 45.3746 12.4014 40.2928 21.9163C39.4636 23.4558 38.5068 25.2519 36.0403 29.9131C33.9565 33.826 33.2761 34.7027 31.6389 35.6221L30.512 36.2421H23.4953H16.4786L15.2454 35.6221C13.9058 34.9379 12.8001 33.8474 12.141 32.5431C11.9071 32.0941 10.8015 28.1171 9.67453 23.7124C8.54761 19.3078 6.9529 13.1284 6.14492 9.96392C5.33694 6.82079 4.67779 4.1053 4.67779 3.93425C4.67779 3.67767 5.57083 3.6349 9.86589 3.6349H15.054L15.4792 5.2813Z" fill="#006675"/>
                <path d="M56.896 7.35668L53.1523 14.1578L55.5635 14.2219L57.9958 14.2861V19.0768C57.9958 22.1352 58.0804 23.8889 58.2073 23.8889C58.3342 23.8889 58.4188 23.8034 58.4188 23.7178C58.4188 23.5467 59.0322 22.3704 62.6489 15.6549C65.1658 10.9711 65.61 10.137 65.61 10.0515C65.61 10.0301 64.5101 9.9873 63.1777 9.9873H60.7454L60.7031 5.28213L60.6396 0.555582L56.896 7.35668Z" fill="#006675"/>
                <path d="M68.8313 12.5C68.7041 12.6922 67.9194 14.134 66.8802 16.0777C65.523 18.5767 64.4626 20.5631 60.1787 28.466L55.9796 36.2621L50.7202 36.3262C46.2242 36.3689 45.4819 36.3262 45.5667 36.0699C45.6304 35.899 47.7935 31.8621 50.3596 27.099C52.9469 22.3359 55.0518 18.4058 55.073 18.2777L51.1019 18.2777C50.9746 18.4272 50.402 19.3884 49.8507 20.4563C49.2993 21.5029 47.7511 24.3864 46.415 26.8641C45.079 29.3204 43.2975 32.5884 42.4917 34.1262C41.6646 35.6427 40.6254 37.6078 40.1588 38.4621L39.3105 40L48.7054 39.9573L58.0791 39.8932L59.6485 37.0097C60.518 35.4291 62.2146 32.2893 63.4022 30.068C64.611 27.8252 66.9863 23.4253 68.7041 20.2641C70.4219 17.1029 72.0549 14.0486 72.3518 13.4719L72.8077 12.5H68.8313Z" fill="#006675"/>
                <path d="M76.0723 14.5662C75.4341 15.7165 73.6473 18.9969 72.0944 21.8726C70.5416 24.7483 67.8613 29.6689 66.1383 32.8428C64.4153 35.9954 62.8412 38.9136 62.6497 39.2971L62.2881 40L69.9034 39.9361C78.0719 39.8509 78.2208 39.8296 80.6245 38.5941C81.7945 37.9764 83.7727 35.9314 84.5811 34.483C85.0916 33.567 85.3894 33.2049 85.4958 33.3966C85.5809 33.567 86.0063 35.1007 86.453 36.8261L87.2613 39.9787H96.6848H106.087L107.044 38.2107C107.576 37.2521 109.448 33.78 111.235 30.4996C113.022 27.2192 114.808 23.9175 115.213 23.1507C115.915 21.8726 116.744 20.3176 119.807 14.7366L120.977 12.6065L114.149 12.5426C109.81 12.5213 106.895 12.5852 106.172 12.7343C103.428 13.2881 100.812 15.2479 99.3863 17.7614C98.9822 18.507 98.578 19.1034 98.4929 19.1034C98.3866 19.1034 97.9398 17.6123 97.4931 15.8017L96.6635 12.5H86.9635H77.2423L76.0723 14.5662ZM94.983 20.5519C96.2168 25.4086 96.4934 25.9198 97.8548 25.9198C98.3015 25.9198 98.8333 25.7494 99.0673 25.5364C99.3013 25.3447 100.216 23.8323 101.109 22.1921C102.96 18.72 103.747 17.7188 105.3 16.8668L106.449 16.2277L110.66 16.1638C112.958 16.1212 114.851 16.1425 114.851 16.2064C114.851 16.2703 113.192 19.359 111.171 23.0868C109.15 26.8145 106.683 31.3304 105.725 33.0984L103.96 36.3575H97.1741C93.4515 36.3575 90.3883 36.2936 90.3883 36.1871C90.3883 36.1019 89.8565 33.993 89.2184 31.4795C87.9633 26.4524 87.6868 25.9198 86.3892 25.9198C85.3469 25.9198 84.7938 26.6015 82.8368 30.2866C80.901 33.9291 79.9651 35.0155 78.0931 35.761C77.0083 36.1871 76.4552 36.251 72.6475 36.3149C68.7122 36.3788 68.4357 36.3575 68.5633 36.0167C68.6909 35.6758 74.6684 24.5991 76.7743 20.8075C77.3273 19.8064 78.1357 18.3579 78.5611 17.5484L79.3482 16.1212L86.6019 16.1638L93.8556 16.2277L94.983 20.5519Z" fill="#006675"/>
                <path d="M76.6233 3.76018H74.0173C73.9989 3.54383 73.9496 3.34756 73.8695 3.17139C73.7925 2.99521 73.6847 2.84376 73.546 2.71704C73.4105 2.58722 73.2457 2.48832 73.0517 2.42032C72.8576 2.34923 72.6374 2.31369 72.3909 2.31369C71.9597 2.31369 71.5947 2.41877 71.2959 2.62895C71.0002 2.83912 70.7753 3.14048 70.6213 3.53301C70.4703 3.92554 70.3949 4.39689 70.3949 4.94705C70.3949 5.52812 70.4719 6.01492 70.6259 6.40746C70.783 6.7969 71.0094 7.09052 71.3051 7.28834C71.6008 7.48306 71.9566 7.58042 72.3724 7.58042C72.6096 7.58042 72.8222 7.55105 73.0101 7.49233C73.198 7.43051 73.3612 7.34243 73.4998 7.22807C73.6385 7.11371 73.7509 6.97616 73.8371 6.81544C73.9265 6.65163 73.9865 6.46773 74.0173 6.26373L76.6233 6.28228C76.5925 6.68408 76.4801 7.09362 76.286 7.51087C76.0919 7.92504 75.8162 8.3083 75.4589 8.66065C75.1047 9.00991 74.6657 9.29118 74.1421 9.50444C73.6184 9.71771 73.0101 9.82434 72.317 9.82434C71.4483 9.82434 70.669 9.63735 69.979 9.26336C69.2921 8.88937 68.7484 8.33921 68.348 7.61287C67.9506 6.88653 67.752 5.99793 67.752 4.94705C67.752 3.89 67.9553 2.99985 68.3619 2.2766C68.7685 1.55026 69.3168 1.00164 70.0068 0.630744C70.6967 0.256757 71.4668 0.0697632 72.317 0.0697632C72.9146 0.0697632 73.4644 0.151669 73.9665 0.315482C74.4686 0.479295 74.9091 0.718832 75.288 1.03409C75.6669 1.34627 75.9718 1.73107 76.2028 2.18851C76.4339 2.64595 76.574 3.16984 76.6233 3.76018Z" fill="#006675"/>
                <path d="M88.7774 4.94705C88.7774 6.00411 88.5726 6.8958 88.1629 7.62214C87.7532 8.34539 87.2003 8.89401 86.5042 9.268C85.808 9.63889 85.0318 9.82434 84.1754 9.82434C83.3129 9.82434 82.5336 9.63735 81.8375 9.26336C81.1444 8.88628 80.593 8.33612 80.1833 7.61287C79.7767 6.88653 79.5734 5.99793 79.5734 4.94705C79.5734 3.89 79.7767 2.99985 80.1833 2.2766C80.593 1.55026 81.1444 1.00164 81.8375 0.630744C82.5336 0.256757 83.3129 0.0697632 84.1754 0.0697632C85.0318 0.0697632 85.808 0.256757 86.5042 0.630744C87.2003 1.00164 87.7532 1.55026 88.1629 2.2766C88.5726 2.99985 88.7774 3.89 88.7774 4.94705ZM86.1345 4.94705C86.1345 4.37834 86.0591 3.89927 85.9081 3.50983C85.7603 3.1173 85.54 2.82058 85.2474 2.61968C84.9578 2.41568 84.6005 2.31369 84.1754 2.31369C83.7504 2.31369 83.3915 2.41568 83.0989 2.61968C82.8093 2.82058 82.5891 3.1173 82.4381 3.50983C82.2903 3.89927 82.2164 4.37834 82.2164 4.94705C82.2164 5.51576 82.2903 5.99638 82.4381 6.38891C82.5891 6.77835 82.8093 7.07507 83.0989 7.27906C83.3915 7.47997 83.7504 7.58042 84.1754 7.58042C84.6005 7.58042 84.9578 7.47997 85.2474 7.27906C85.54 7.07507 85.7603 6.77835 85.9081 6.38891C86.0591 5.99638 86.1345 5.51576 86.1345 4.94705Z" fill="#006675"/>
                <path d="M95.4286 9.69453H91.7876V0.199577H95.3916C96.365 0.199577 97.2059 0.389662 97.9144 0.769831C98.626 1.14691 99.1743 1.69089 99.5593 2.40178C99.9474 3.10957 100.141 3.95799 100.141 4.94705C100.141 5.93611 99.949 6.78608 99.5639 7.49697C99.1789 8.20476 98.6337 8.74874 97.9283 9.12891C97.2229 9.50599 96.3897 9.69453 95.4286 9.69453ZM94.3566 7.50624H95.3362C95.8044 7.50624 96.2033 7.43051 96.5329 7.27906C96.8656 7.12761 97.1182 6.86644 97.2907 6.49554C97.4662 6.12465 97.554 5.60848 97.554 4.94705C97.554 4.28562 97.4647 3.76946 97.286 3.39856C97.1105 3.02766 96.8517 2.76649 96.5098 2.61504C96.171 2.46359 95.7551 2.38787 95.2623 2.38787H94.3566V7.50624Z" fill="#006675"/>
                <path d="M103.152 9.69453V0.199577H109.972V2.2766H105.721V3.90854H109.62V5.98556H105.721V7.61751H109.953V9.69453H103.152Z" fill="#006675"/>
                <path d="M113.067 9.69453V0.199577H117.152C117.854 0.199577 118.469 0.327845 118.995 0.584382C119.522 0.840919 119.932 1.21027 120.224 1.69244C120.517 2.1746 120.663 2.75258 120.663 3.42638C120.663 4.10635 120.512 4.6797 120.21 5.14641C119.912 5.61312 119.491 5.96547 118.949 6.20346C118.41 6.44146 117.78 6.56045 117.059 6.56045H114.62V4.55761H116.542C116.844 4.55761 117.101 4.52052 117.313 4.44634C117.529 4.36907 117.694 4.24698 117.808 4.08008C117.925 3.91318 117.983 3.69528 117.983 3.42638C117.983 3.15439 117.925 2.93339 117.808 2.7634C117.694 2.59031 117.529 2.46359 117.313 2.38323C117.101 2.29978 116.844 2.25805 116.542 2.25805H115.636V9.69453H113.067ZM118.612 5.33649L120.978 9.69453H118.187L115.876 5.33649H118.612Z" fill="#006675"/>
              </g>
              <defs>
                <clipPath id="clip0_2028_1728">
                  <rect width="121" height="40" fill="white"/>
                </clipPath>
              </defs>
            </svg>
          </div>
        </div>
      </div>

      <nav className="flex-1 px-4 py-4 space-y-1">
        {navItems.map((item) => {
          const Icon = item.icon;
          const isActive = activeTab === item.id;
          return (
            <button
              key={item.id}
              onClick={() => {
                setActiveTab(item.id);
                setMobileOpen(false);
              }}
              className={`w-full flex items-center gap-3 px-4 py-3 rounded-2xl transition-all duration-200 text-sm font-medium ${
                isActive
                  ? 'bg-slate-50 text-brand-dark shadow-sm'
                  : 'text-slate-500 hover:bg-slate-50 hover:text-brand-dark'
              }`}
            >
              <Icon size={18} className={isActive ? 'text-brand-dark' : 'text-slate-400'} />
              {item.label}
            </button>
          );
        })}
      </nav>

      <div className="p-4 m-4 bg-slate-50 rounded-2xl flex items-center justify-between gap-3 border border-slate-100">
        <div className="flex items-center gap-3 min-w-0">
          <div className="w-10 h-10 rounded-full bg-brand-100 text-brand-600 flex items-center justify-center font-bold text-sm shrink-0">
            {user?.full_name?.charAt(0) || 'U'}
          </div>
          <div className="min-w-0">
            <div className="font-semibold text-sm text-brand-dark truncate">{user?.full_name}</div>
            <div className="text-xs text-slate-500 font-bold uppercase tracking-wider truncate">{user?.role}</div>
          </div>
        </div>
        <button 
          onClick={logout}
          className="p-2 text-slate-400 hover:text-rose-500 hover:bg-rose-50 rounded-xl transition-colors"
          title="Выйти"
        >
          <Power size={18} />
        </button>
      </div>
    </div>
  );

  return (
    <>
      {/* Mobile Backdrop */}
      {mobileOpen && (
        <div
          className="fixed inset-0 bg-brand-dark/20 backdrop-blur-sm z-40 lg:hidden"
          onClick={() => setMobileOpen(false)}
        />
      )}
      
      {/* Sidebar */}
      <aside
        className={`fixed lg:sticky top-0 left-0 h-screen w-72 z-50 transition-transform duration-300 transform ${
          mobileOpen ? 'translate-x-0' : '-translate-x-full lg:translate-x-0'
        }`}
      >
        {content}
      </aside>
    </>
  );
}

const chartDataByPeriod = {
  'Сегодня': [
    { name: '00:00', revenue: 0, profit: 0 },
    { name: '04:00', revenue: 800, profit: 520 },
    { name: '08:00', revenue: 2100, profit: 1360 },
    { name: '12:00', revenue: 4500, profit: 2900 },
    { name: '16:00', revenue: 6800, profit: 4400 },
    { name: '20:00', revenue: 8200, profit: 5300 },
    { name: '23:59', revenue: 9100, profit: 5800 },
  ],
  '7 дней': [
    { name: 'Пн', revenue: 6500, profit: 4200 },
    { name: 'Вт', revenue: 7800, profit: 5000 },
    { name: 'Ср', revenue: 7200, profit: 4500 },
    { name: 'Чт', revenue: 9100, profit: 5800 },
    { name: 'Пт', revenue: 11200, profit: 7200 },
    { name: 'Сб', revenue: 13500, profit: 8600 },
    { name: 'Вс', revenue: 7500, profit: 4800 },
  ],
  '30 дней': [
    { name: '1 нед', revenue: 45000, profit: 29000 },
    { name: '2 нед', revenue: 52000, profit: 33500 },
    { name: '3 нед', revenue: 48000, profit: 31000 },
    { name: '4 нед', revenue: 63000, profit: 40700 },
  ],
  'Год': [
    { name: 'Янв', revenue: 180000, profit: 116000 },
    { name: 'Фев', revenue: 165000, profit: 106000 },
    { name: 'Мар', revenue: 195000, profit: 126000 },
    { name: 'Апр', revenue: 210000, profit: 135000 },
    { name: 'Май', revenue: 0, profit: 0 },
    { name: 'Июн', revenue: 0, profit: 0 },
    { name: 'Июл', revenue: 0, profit: 0 },
    { name: 'Авг', revenue: 0, profit: 0 },
    { name: 'Сен', revenue: 0, profit: 0 },
    { name: 'Окт', revenue: 0, profit: 0 },
    { name: 'Ноя', revenue: 0, profit: 0 },
    { name: 'Дек', revenue: 0, profit: 0 },
  ],
  'За все время': [
    { name: '2023', revenue: 450000, profit: 290000 },
    { name: '2024', revenue: 820000, profit: 535000 },
    { name: '2025', revenue: 1150000, profit: 750000 },
    { name: '2026', revenue: 740000, profit: 483000 },
  ],
};

function EconomyPage() {
  const { stations } = useMqttStore();
  const [selectedPeriod, setSelectedPeriod] = useState('7 дней');
  const economyChartData = chartDataByPeriod[selectedPeriod];
  
  // Load actuals from localStorage or fallback
  const getActuals = () => {
    try {
       const stored = localStorage.getItem('economySettings');
       if (stored) return JSON.parse(stored);
    } catch(e) {}
    return { buy: 5.0, sell: 15.0, acq: 2.0 };
  };
  const [actuals, setActuals] = useState(getActuals());
  
  // States for interactive math (initially matching actuals)
  const [buyPrice, setBuyPrice] = useState(actuals.buy);
  const [sellPrice, setSellPrice] = useState(actuals.sell);
  const [acqRate, setAcqRate] = useState(actuals.acq);

  const handleActualChange = (key, val) => {
    const newActuals = { ...actuals, [key]: val };
    setActuals(newActuals);
    localStorage.setItem('economySettings', JSON.stringify(newActuals));
  };
  
  // Рассчитываем динамику от онлайн станций
  const dynamicKwh = Object.values(stations).reduce((sum, s) => sum + (s.current_kwh || 0), 0);
  
  const baseVolume = 4200;

  const currentVolume = baseVolume + dynamicKwh;
  const currentRevenue = currentVolume * sellPrice;
  const currentEE = currentVolume * buyPrice;
  const currentBank = currentRevenue * (acqRate / 100);
  const currentCOGS = currentEE + currentBank;
  const currentNet = currentRevenue - currentCOGS;
  const currentMargin = currentRevenue > 0 ? ((currentNet / currentRevenue) * 100).toFixed(1) : 0;

  return (
    <div className="max-w-6xl mx-auto space-y-6">
      <div className="flex flex-col sm:flex-row justify-between items-start sm:items-center gap-4 mb-8">
        <div>
          <h1 className="text-2xl font-bold text-brand-dark">Экономика и Дашборд</h1>
          <p className="text-slate-500 text-sm">Анализ P&L и юнит-экономики сети</p>
        </div>
        <div className="flex bg-white rounded-full p-1 shadow-sm text-sm">
          {['Сегодня', '7 дней', '30 дней', 'Год', 'За все время'].map((period) => (
            <button
              key={period}
              onClick={() => setSelectedPeriod(period)}
              className={`px-4 py-1.5 rounded-full font-medium transition-colors ${
                selectedPeriod === period ? 'bg-brand-dark text-white' : 'text-slate-600 hover:bg-slate-50'
              }`}
            >
              {period}
            </button>
          ))}
        </div>
      </div>

      <div className="bg-white p-6 rounded-3xl shadow-sm space-y-6">
        <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
          <div>
            <div className="flex justify-between text-xs font-bold text-slate-400 tracking-wider mb-2 uppercase">
              <span>Закупка ЭЭ (₽/кВт·ч)</span>
              <span className="text-brand-dark">{buyPrice.toFixed(1)} ₽</span>
            </div>
            <input 
              type="range" 
              min="1" max="15" step="0.1" 
              value={buyPrice} 
              onChange={(e) => setBuyPrice(parseFloat(e.target.value))}
              style={{ background: `linear-gradient(to right, #035062 ${((buyPrice - 1) / 14) * 100}%, #f1f5f9 ${((buyPrice - 1) / 14) * 100}%)` }}
              className="w-full h-2 rounded-lg appearance-none cursor-pointer transition-all [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-4 [&::-webkit-slider-thumb]:h-4 [&::-webkit-slider-thumb]:bg-[#035062] [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:border-2 [&::-webkit-slider-thumb]:border-white [&::-webkit-slider-thumb]:shadow-md [&::-moz-range-thumb]:w-4 [&::-moz-range-thumb]:h-4 [&::-moz-range-thumb]:bg-[#035062] [&::-moz-range-thumb]:rounded-full [&::-moz-range-thumb]:border-2 [&::-moz-range-thumb]:border-white [&::-moz-range-thumb]:shadow-md"
            />
            <div className="mt-4 flex items-center justify-between text-sm">
              <span className="font-bold text-slate-500">Текущая закупка</span>
              <input 
                type="number" 
                step="0.1"
                min="0"
                value={actuals.buy}
                onChange={(e) => handleActualChange('buy', parseFloat(e.target.value) || 0)}
                className="w-16 text-right font-bold bg-slate-50 border-2 border-slate-100 rounded-lg px-2 py-1 outline-none focus:border-brand-300 transition-colors"
              />
            </div>
          </div>
          <div>
            <div className="flex justify-between text-xs font-bold text-slate-400 tracking-wider mb-2 uppercase">
              <span>Продажа (₽/кВт·ч)</span>
              <span className="text-brand-500">{sellPrice.toFixed(1)} ₽</span>
            </div>
            <input 
              type="range" 
              min="5" max="30" step="0.5" 
              value={sellPrice} 
              onChange={(e) => setSellPrice(parseFloat(e.target.value))}
              style={{ background: `linear-gradient(to right, #035062 ${((sellPrice - 5) / 25) * 100}%, #f1f5f9 ${((sellPrice - 5) / 25) * 100}%)` }}
              className="w-full h-2 rounded-lg appearance-none cursor-pointer transition-all [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-4 [&::-webkit-slider-thumb]:h-4 [&::-webkit-slider-thumb]:bg-[#035062] [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:border-2 [&::-webkit-slider-thumb]:border-white [&::-webkit-slider-thumb]:shadow-md [&::-moz-range-thumb]:w-4 [&::-moz-range-thumb]:h-4 [&::-moz-range-thumb]:bg-[#035062] [&::-moz-range-thumb]:rounded-full [&::-moz-range-thumb]:border-2 [&::-moz-range-thumb]:border-white [&::-moz-range-thumb]:shadow-md"
            />
            <div className="mt-4 flex items-center justify-between text-sm">
              <span className="font-bold text-slate-500">Текущая продажа</span>
              <input 
                type="number" 
                step="0.1"
                min="0"
                value={actuals.sell}
                onChange={(e) => handleActualChange('sell', parseFloat(e.target.value) || 0)}
                className="w-16 text-right font-bold bg-slate-50 border-2 border-slate-100 rounded-lg px-2 py-1 outline-none focus:border-brand-300 transition-colors"
              />
            </div>
          </div>
          <div>
            <div className="flex justify-between text-xs font-bold text-slate-400 tracking-wider mb-2 uppercase">
              <span>Эквайринг (%)</span>
              <span className="text-rose-500">{acqRate.toFixed(1)}%</span>
            </div>
            <input 
              type="range" 
              min="0.1" max="5" step="0.1" 
              value={acqRate} 
              onChange={(e) => setAcqRate(parseFloat(e.target.value))}
              style={{ background: `linear-gradient(to right, #035062 ${((acqRate - 0.1) / 4.9) * 100}%, #f1f5f9 ${((acqRate - 0.1) / 4.9) * 100}%)` }}
              className="w-full h-2 rounded-lg appearance-none cursor-pointer transition-all [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-4 [&::-webkit-slider-thumb]:h-4 [&::-webkit-slider-thumb]:bg-[#035062] [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:border-2 [&::-webkit-slider-thumb]:border-white [&::-webkit-slider-thumb]:shadow-md [&::-moz-range-thumb]:w-4 [&::-moz-range-thumb]:h-4 [&::-moz-range-thumb]:bg-[#035062] [&::-moz-range-thumb]:rounded-full [&::-moz-range-thumb]:border-2 [&::-moz-range-thumb]:border-white [&::-moz-range-thumb]:shadow-md"
            />
            <div className="mt-4 flex items-center justify-between text-sm">
              <span className="font-bold text-slate-500">Текущий эквайринг</span>
              <input 
                type="number" 
                step="0.1"
                min="0"
                value={actuals.acq}
                onChange={(e) => handleActualChange('acq', parseFloat(e.target.value) || 0)}
                className="w-16 text-right font-bold bg-slate-50 border-2 border-slate-100 rounded-lg px-2 py-1 outline-none focus:border-brand-300 transition-colors"
              />
            </div>
          </div>
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        <div className="bg-white p-6 rounded-3xl shadow-sm transition-all duration-500">
          <div className="text-xs font-bold text-slate-400 tracking-wider uppercase mb-1">Выручка (Gross)</div>
          <div className="text-4xl font-black text-brand-dark tracking-tight mb-4">{currentRevenue.toLocaleString('ru-RU')} ₽</div>
          <div className="inline-block px-3 py-1 bg-slate-50 text-slate-600 text-xs font-bold rounded-lg uppercase tracking-wider">
            Объем: {currentVolume.toFixed(1)} кВт·ч
          </div>
        </div>

        <div className="bg-white p-6 rounded-3xl shadow-sm relative overflow-hidden transition-all duration-500">
          <div className="text-xs font-bold text-slate-400 tracking-wider uppercase mb-1">Расходы (COGS)</div>
          <div className="text-4xl font-black text-rose-500 tracking-tight mb-4">-{currentCOGS.toLocaleString('ru-RU')} ₽</div>
          <div className="flex gap-2">
             <div className="px-3 py-1 text-rose-500 bg-rose-50 text-xs font-bold rounded-lg uppercase tracking-wider">
               ЭЭ: {currentEE.toLocaleString('ru-RU')} ₽
             </div>
             <div className="px-3 py-1 text-rose-500 bg-rose-50 text-xs font-bold rounded-lg uppercase tracking-wider">
               Банк: {currentBank.toLocaleString('ru-RU')} ₽
             </div>
          </div>
        </div>

        <div className="bg-white p-6 rounded-3xl shadow-sm relative overflow-hidden transition-all duration-500">
          <div className="absolute inset-0 bg-gradient-to-br from-brand-light to-transparent"></div>
          <div className="relative">
            <div className="text-xs font-bold text-brand tracking-wider uppercase mb-1">Чистая прибыль (Net)</div>
            <div className="text-4xl font-black text-brand tracking-tight mb-4">+{currentNet.toLocaleString('ru-RU')} ₽</div>
            <div className="inline-block px-3 py-1 text-brand-dark font-bold text-xs rounded-lg uppercase tracking-wider bg-brand-light">
              Маржинальность: {currentMargin}%
            </div>
          </div>
        </div>
      </div>

      <div className="bg-white p-6 rounded-3xl shadow-sm mt-6">
        <div className="flex justify-between items-center mb-8">
          <h2 className="text-xs font-bold text-slate-400 tracking-wider uppercase">Динамика маржинального разрыва</h2>
          <div className="flex items-center gap-4 text-xs font-semibold text-slate-500">
            <div className="flex items-center gap-2"><div className="w-3 h-3 rounded bg-slate-200"></div> Выручка</div>
            <div className="flex items-center gap-2"><div className="w-4 h-0.5 bg-brand-400"></div> Прибыль</div>
          </div>
        </div>
        <div className="h-[300px] w-full">
          <ResponsiveContainer width="100%" height="100%">
            <ComposedChart data={economyChartData} margin={{ top: 20, right: 0, left: -20, bottom: 0 }}>
              <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="#f1f5f9" />
              <XAxis dataKey="name" axisLine={false} tickLine={false} tick={{fill: '#94a3b8', fontSize: 12}} dy={10} />
              <YAxis axisLine={false} tickLine={false} tick={{fill: '#94a3b8', fontSize: 12}} />
              <Tooltip cursor={{fill: '#f8fafc'}} contentStyle={{borderRadius: '12px', border: 'none', boxShadow: '0 4px 6px -1px rgb(0 0 0 / 0.1)'}} />
              <Bar dataKey="revenue" name="Выручка" fill="#e2e8f0" radius={[4, 4, 0, 0]} barSize={32} />
              <Line type="monotone" dataKey="profit" name="Прибыль" stroke="#10A8AB" strokeWidth={2.5} dot={{ r: 4, fill: '#10A8AB', strokeWidth: 2, stroke: '#fff' }} activeDot={{ r: 6 }} />
            </ComposedChart>
          </ResponsiveContainer>
        </div>
      </div>
    </div>
  );
}

function TransactionsPage() {
  const { payments } = useMqttStore();
  const [dbPayments, setDbPayments] = useState([]);
  
  useEffect(() => {
    api.get('/payments?limit=50').then(setDbPayments).catch(console.error);
  }, []);

  const allTransactions = [...payments, ...dbPayments].map(tx => ({
    id: tx.id || tx.transaction_id,
    type: tx.type || 'card',
    user: tx.user || 'Оплата на ЭЗС',
    date: tx.date || (tx.occurred_at ? new Date(tx.occurred_at).toLocaleString() : ''),
    station: tx.station || `ST-${tx.station_id}`,
    kwh: Number(tx.kwh || tx.kwh_spent || 0),
    sum: Number(tx.sum !== undefined ? tx.sum : (tx.amount_paid || 0)),
    change: Number(tx.change !== undefined ? tx.change : (tx.refund_amount !== undefined ? tx.refund_amount : (Math.random() > 0.6 ? Math.random() * 200 : 0))),
    profit: Number(tx.profit !== undefined ? tx.profit : ((tx.amount_paid || 0) * 0.4)),
    status: tx.status || 'success'
  }));

  return (
    <div className="max-w-4xl mx-auto space-y-6">
      <div className="flex flex-col sm:flex-row justify-between items-start sm:items-center gap-4 mb-8">
        <div>
          <h1 className="text-2xl font-bold text-brand-dark">Лента транзакций</h1>
          <p className="text-slate-500 text-sm uppercase tracking-wider font-bold mt-1">Детальный учет и аналитика</p>
        </div>
        <div className="bg-slate-100 text-slate-500 text-xs font-bold px-4 py-2 rounded-full uppercase tracking-wider">
          {allTransactions.length} записей
        </div>
      </div>

      <div className="space-y-4">
        {allTransactions.map((tx, idx) => (
          <div key={idx} className="bg-white p-5 rounded-2xl shadow-sm flex flex-col sm:flex-row items-start sm:items-center justify-between gap-4 transition-all hover:shadow-md">
            <div className="flex items-center gap-4">
              <div className={`w-12 h-12 rounded-2xl flex items-center justify-center shrink-0 ${tx.type === 'card' ? 'bg-brand-50 text-brand-500' : 'bg-slate-50 text-slate-400'}`}>
                {tx.type === 'card' ? <CreditCard size={20} /> : <User size={20} />}
              </div>
              <div className="min-w-0">
                <div className="flex items-center gap-2 mb-1">
                  <span className="font-bold text-brand-dark whitespace-nowrap">{tx.user}</span>
                  <span className="text-[10px] font-bold text-brand-600 bg-brand-50 px-2 py-0.5 rounded-full tracking-wider">{tx.id}</span>
                </div>
                <div className="text-xs text-slate-500 font-medium">
                  {tx.date} • {tx.station}
                </div>
              </div>
            </div>

            <div className="flex items-center gap-6 w-full sm:w-auto justify-between sm:justify-end pt-4 sm:pt-0 flex-wrap">
              <div className="text-right">
                <div className="text-[10px] uppercase tracking-wider font-bold text-slate-400 mb-1">Энергия</div>
                <div className="font-black text-brand-dark">{tx.kwh} кВт·ч</div>
              </div>
              <div className="text-right">
                <div className="text-[10px] uppercase tracking-wider font-bold text-slate-400 mb-1">Сумма</div>
                <div className="font-black text-brand-dark">{tx.sum.toFixed(1)} ₽</div>
              </div>
              {tx.change > 0 && (
                <div className="text-right">
                  <div className="text-[10px] uppercase tracking-wider font-bold text-slate-400 mb-1">Сдача</div>
                  <div className="font-black text-amber-500">{tx.change.toFixed(2)} ₽</div>
                </div>
              )}
              <div className="text-right pl-4">
                 <div className="text-[10px] uppercase tracking-wider font-bold text-slate-400 mb-1">Чистая прибыль</div>
                 <div className={`font-black ${tx.profit >= 0 ? 'text-brand-500' : 'text-rose-500'}`}>
                   {tx.profit > 0 ? '+' : ''}{tx.profit.toFixed(2)} ₽
                 </div>
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

// === Telemetry Helper Functions ===
function formatUptime(seconds) {
  if (!seconds || seconds <= 0) return '—';
  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;
  if (d > 0) return `${d}д ${h}ч`;
  if (h > 0) return `${h}ч ${m}м`;
  return `${m}м ${s}с`;
}

function formatMemory(bytes) {
  if (!bytes || bytes <= 0) return '—';
  if (bytes >= 1048576) return `${(bytes / 1048576).toFixed(1)} МБ`;
  return `${Math.round(bytes / 1024)} КБ`;
}

function WifiSignalBars({ rssi }) {
  const strength = !rssi ? 0 : rssi >= -50 ? 4 : rssi >= -60 ? 3 : rssi >= -70 ? 2 : rssi >= -80 ? 1 : 0;
  const barColor = strength >= 3 ? 'bg-emerald-500' : strength >= 2 ? 'bg-amber-500' : 'bg-rose-500';
  return (
    <div className="flex items-end gap-[2px] h-3.5">
      {[1, 2, 3, 4].map(i => (
        <div
          key={i}
          className={`w-[3px] rounded-sm transition-all duration-300 ${i <= strength ? barColor : 'bg-slate-200'}`}
          style={{ height: `${i * 25}%` }}
        />
      ))}
    </div>
  );
}

function getWifiColor(rssi) {
  if (!rssi) return 'text-slate-300';
  if (rssi >= -50) return 'text-emerald-500';
  if (rssi >= -60) return 'text-emerald-400';
  if (rssi >= -70) return 'text-amber-500';
  return 'text-rose-500';
}

function StationCard({ assetBase, getAssetData }) {
  const asset = getAssetData(assetBase);
  const mqttData = useMqttStation(asset.id.toString());

  const isCharging = mqttData.chargingActive;
  const currentPower = mqttData.currentPower || 0;
  const sessionKwh = mqttData.currentKwh || 0;
  const activePistol = mqttData.activePistol;
  const uptime = mqttData.uptime || 0;
  const freeMemory = mqttData.freeMemory || 0;
  const wifiTerminal = mqttData.wifiRssiTerminal;
  const wifiEsp = mqttData.wifiRssiEsp32;

  return (
    <div className={`bg-white rounded-3xl shadow-sm p-6 relative overflow-hidden transition-all duration-300 ${mqttData.meterError ? 'border-2 border-rose-400 ring-4 ring-rose-50' : ''}`}>
      {mqttData.meterError && (
        <div className="absolute top-0 right-0 p-4 font-black text-rose-500 opacity-20 text-6xl pointer-events-none">
          METER ERR
        </div>
      )}

      {/* Header — unchanged */}
      <div className="flex justify-between items-start mb-5">
        <div className="flex gap-4">
          <div className="w-10 h-10 rounded-full bg-brand-50 text-brand-500 flex items-center justify-center shrink-0">
            <MapPin size={20} />
          </div>
          <div>
            <h3 className="font-bold text-lg text-brand-dark">{asset.name}</h3>
            <p className="text-xs text-slate-400 font-medium">{asset.location}</p>
          </div>
        </div>
        <div className="text-[10px] font-bold text-brand-600 tracking-wider uppercase px-2 py-1 bg-brand-50 rounded-lg">
          {asset.status}
        </div>
      </div>

      {/* === Charging Status Banner === */}
      {isCharging ? (
        <div className="relative bg-gradient-to-br from-brand-50 via-emerald-50/50 to-brand-50/80 p-4 rounded-2xl border border-brand-100/60 mb-4">
          <div className="flex items-center justify-between mb-3">
            <div className="flex items-center gap-2">
              <span className="relative flex h-2.5 w-2.5">
                <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span>
                <span className="relative inline-flex rounded-full h-2.5 w-2.5 bg-emerald-500"></span>
              </span>
              <span className="text-[10px] font-bold text-emerald-600 uppercase tracking-wider">Зарядка активна</span>
            </div>
            {activePistol && (
              <div className="text-[10px] font-bold text-brand-700 bg-brand-100 px-2 py-0.5 rounded-md uppercase tracking-wider">
                Пистолет {activePistol}
              </div>
            )}
          </div>
          <div className="flex items-end gap-8">
            <div>
              <div className="text-3xl font-black text-brand-dark tracking-tight leading-none">{currentPower.toFixed(2)}</div>
              <div className="text-[10px] font-bold text-slate-400 uppercase tracking-wider mt-1">кВт</div>
            </div>
            <div>
              <div className="text-xl font-black text-brand-600 leading-none">{sessionKwh.toFixed(1)}</div>
              <div className="text-[10px] font-bold text-slate-400 uppercase tracking-wider mt-1">кВт·ч сессия</div>
            </div>
          </div>
        </div>
      ) : (
        <div className="flex items-center gap-3 bg-slate-50/80 p-3.5 rounded-xl mb-4">
          <div className="w-2 h-2 rounded-full bg-slate-300"></div>
          <span className="text-xs font-semibold text-slate-400">Ожидание</span>
          <span className="text-xs font-bold text-slate-300 ml-auto">0 кВт</span>
        </div>
      )}

      {/* === System Telemetry Grid === */}
      <div className="grid grid-cols-2 sm:grid-cols-4 gap-2 mb-4">
        <div className="bg-slate-50 p-2.5 rounded-xl">
          <div className="flex items-center gap-1 mb-1">
            <Zap size={10} className="text-amber-500" />
            <span className="text-[9px] font-bold text-slate-400 uppercase tracking-wider">Мощность</span>
          </div>
          <div className="text-sm font-black text-brand-dark">{currentPower.toFixed(1)} <span className="text-[9px] font-bold text-slate-400">кВт</span></div>
        </div>
        <div className="bg-slate-50 p-2.5 rounded-xl">
          <div className="flex items-center gap-1 mb-1">
            <Activity size={10} className="text-emerald-500" />
            <span className="text-[9px] font-bold text-slate-400 uppercase tracking-wider">Сессия</span>
          </div>
          <div className="text-sm font-black text-brand-dark">{sessionKwh.toFixed(1)} <span className="text-[9px] font-bold text-slate-400">кВт·ч</span></div>
        </div>
        <div className="bg-slate-50 p-2.5 rounded-xl">
          <div className="flex items-center gap-1 mb-1">
            <Clock size={10} className="text-blue-500" />
            <span className="text-[9px] font-bold text-slate-400 uppercase tracking-wider">Аптайм</span>
          </div>
          <div className="text-sm font-black text-brand-dark">{formatUptime(uptime)}</div>
        </div>
        <div className="bg-slate-50 p-2.5 rounded-xl">
          <div className="flex items-center gap-1 mb-1">
            <Cpu size={10} className="text-violet-500" />
            <span className="text-[9px] font-bold text-slate-400 uppercase tracking-wider">Память ESP</span>
          </div>
          <div className="text-sm font-black text-brand-dark">{formatMemory(freeMemory)}</div>
        </div>
      </div>

      {/* === WiFi Signals === */}
      <div className="flex gap-2 mb-5">
        <div className="flex-1 bg-slate-50 p-2.5 rounded-xl">
          <div className="flex items-center justify-between">
            <div>
              <div className="text-[9px] font-bold text-slate-400 uppercase tracking-wider mb-1">WiFi Терминал</div>
              <div className="flex items-center gap-2">
                <WifiSignalBars rssi={wifiTerminal} />
                <span className="text-xs font-bold text-brand-dark">{wifiTerminal ? `${wifiTerminal}` : '—'} <span className="text-[9px] text-slate-400">dBm</span></span>
              </div>
            </div>
            <Wifi size={14} className={getWifiColor(wifiTerminal)} />
          </div>
        </div>
        <div className="flex-1 bg-slate-50 p-2.5 rounded-xl">
          <div className="flex items-center justify-between">
            <div>
              <div className="text-[9px] font-bold text-slate-400 uppercase tracking-wider mb-1">WiFi ESP32</div>
              <div className="flex items-center gap-2">
                <WifiSignalBars rssi={wifiEsp} />
                <span className="text-xs font-bold text-brand-dark">{wifiEsp ? `${wifiEsp}` : '—'} <span className="text-[9px] text-slate-400">dBm</span></span>
              </div>
            </div>
            <Wifi size={14} className={getWifiColor(wifiEsp)} />
          </div>
        </div>
      </div>

      {/* Pistols — unchanged */}
      <div className="space-y-3 mb-6">
        <div className="flex justify-between items-center py-2">
          <div className="flex items-center gap-2 text-sm font-semibold text-slate-700">
            <Zap size={14} className="text-slate-400" /> Пистолет A (Type 2)
          </div>
          <div className="text-[10px] font-bold uppercase tracking-wider text-slate-500 bg-slate-100 px-2 py-1 rounded-md">
            {asset.plugA}
          </div>
        </div>
        <div className="flex justify-between items-center py-2 h-10">
          <div className="flex items-center gap-2 text-sm font-semibold text-slate-700">
            <Zap size={14} className="text-brand-500" /> Пистолет B (CCS2)
          </div>
          {asset.plugB === 'Зарядка' ? (
            <div className="flex items-center gap-3">
              <div className="text-right">
                <div className="text-[10px] font-bold text-brand">{asset.progress}%</div>
                <div className="text-[9px] text-slate-400">{asset.etaMinutes} мин</div>
              </div>
              <div className="relative w-8 h-8">
                <svg className="w-8 h-8 transform -rotate-90">
                  <circle cx="16" cy="16" r="14" stroke="currentColor" strokeWidth="3" fill="transparent" className="text-slate-100" />
                  <circle cx="16" cy="16" r="14" stroke="currentColor" strokeWidth="3" fill="transparent" strokeDasharray={14 * 2 * Math.PI} strokeDashoffset={14 * 2 * Math.PI - (asset.progress / 100) * 14 * 2 * Math.PI} className="text-brand transition-all duration-500" />
                </svg>
              </div>
            </div>
          ) : (
            <div className="text-[10px] font-bold uppercase tracking-wider text-orange-600 bg-orange-50 px-2 py-1 rounded-md">
              {asset.plugB}
            </div>
          )}
        </div>
      </div>

      {/* Buttons — unchanged */}
      <div className="grid grid-cols-2 gap-2 sm:gap-3">
        <button onClick={async () => {
          try {
            await api.post('/free-charging/start', { station_id: assetBase.id });
          } catch (e) {
            alert(e.message);
          }
        }} className="flex flex-col items-center justify-center py-4 sm:py-2.5 rounded-xl font-bold text-xs uppercase tracking-wider text-brand-dark bg-brand-light hover:bg-[#d6f0f1] active:scale-95 transition-all min-h-[56px] shadow-sm">
          <Play size={20} className="mb-1 sm:hidden text-brand" />
          <div>Free Режим</div>
          <div className="text-[9px] lowercase font-normal opacity-70 mt-0.5 tracking-normal">На одну зарядку</div>
        </button>
        <button onClick={() => mqttData.restart()} className="flex flex-col items-center justify-center py-4 sm:py-2.5 rounded-xl font-bold text-xs uppercase tracking-wider text-brand-light bg-brand hover:bg-brand-dark active:scale-95 transition-all min-h-[56px] shadow-sm">
          <RotateCcw size={20} className="mb-1 sm:hidden" />
          Reboot
        </button>
      </div>
    </div>
  );
}

function AssetsPage() {
  const { stations } = useMqttStore();
  const [assets, setAssets] = useState([]);

  useEffect(() => {
    api.get('/stations').then(data => {
      let mappedAssets = data.map(s => ({
        ...s,
        location: s.address?.full_address || s.address?.city || 'Неизвестно',
        hw: 'Connected', sm: 'Active', plugA: 'Свободен', plugB: 'Зарядка'
      }));
      
      // Move "Livable charging" to the front
      const livableIdx = mappedAssets.findIndex(s => s.name?.toLowerCase().includes('livable'));
      if (livableIdx > 0) {
        const [livableStation] = mappedAssets.splice(livableIdx, 1);
        mappedAssets.unshift(livableStation);
      }
      
      setAssets(mappedAssets);
    }).catch(console.error);
  }, []);

  const getAssetData = (asset) => {
    // Временно мапим id ассета к '1', '2' и т.д.
    const stationId = asset.id.toString();
    const stationData = stations[stationId];
    if (!stationData) return asset;

    return {
      ...asset,
      status: stationData.isOnline ? 'ONLINE' : 'OFFLINE',
      plugB: stationData.charging_active ? 'Зарядка' : 'Свободен',
      currentPower: stationData.current_power || 0,
      progress: stationData.progress || 0,
      etaMinutes: stationData.eta_minutes || 0
    };
  };

  return (
    <div className="max-w-6xl mx-auto space-y-6">
      <div className="flex flex-col sm:flex-row justify-between items-start sm:items-center gap-4 mb-8">
        <h1 className="text-2xl font-bold text-brand-dark">Управление активами</h1>
        <button className="bg-brand-dark text-white px-5 py-2.5 rounded-xl font-bold text-sm flex items-center gap-2 hover:bg-slate-800 transition-colors shadow-lg shadow-slate-200">
          <Activity size={16} /> Отправить тарифы по MQTT
        </button>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {assets.map((assetBase) => (
          <StationCard key={assetBase.id} assetBase={assetBase} getAssetData={getAssetData} />
        ))}
      </div>
    </div>
  );
}

function MobileTabBar({ activeTab, setActiveTab, user }) {
  const tabs = [
    { id: 'economy', label: 'Экономика', icon: LayoutDashboard, roles: ['super_admin', 'owner'] },
    { id: 'transactions', label: 'Транзакции', icon: History, roles: ['super_admin', 'owner'] },
    { id: 'assets', label: 'Колонки', icon: Zap, roles: ['super_admin', 'owner', 'employee'] },
    { id: 'system', label: 'Настройки', icon: Settings, roles: ['super_admin', 'owner'] }
  ].filter(tab => tab.roles.includes(user?.role));

  return (
    <nav className="lg:hidden fixed bottom-0 left-0 right-0 z-40 bg-white px-2 pb-[env(safe-area-inset-bottom)] shadow-[0_-4px_6px_-1px_rgba(0,0,0,0.05)]">
      <div className="flex justify-around">
        {tabs.map((tab) => {
          const Icon = tab.icon;
          const isActive = activeTab === tab.id;
          return (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              className={`flex flex-col items-center py-2.5 px-3 min-w-[64px] transition-colors ${
                isActive ? 'text-brand-dark' : 'text-slate-400'
              }`}
            >
              <Icon size={22} className={isActive ? 'text-brand-500' : 'text-slate-400'} />
              <span className={`text-[10px] mt-1 font-bold tracking-wider ${
                isActive ? 'text-brand-dark' : 'text-slate-400'
              }`}>
                {tab.label}
              </span>
              {isActive && (
                <div className="w-5 h-0.5 bg-brand-500 rounded-full mt-1" />
              )}
            </button>
          );
        })}
      </div>
    </nav>
  );
}

function DashboardLayout() {
  const { user, logout } = useAuth();
  
  // Определяем дефолтную вкладку по роли
  const initialTab = user?.role === 'employee' ? 'assets' : 'economy';
  const [activeTab, setActiveTab] = useState(initialTab);
  const [mobileOpen, setMobileOpen] = useState(false);

  // Fallback если employee оказался на economy
  useEffect(() => {
    if (user?.role === 'employee' && (activeTab === 'economy' || activeTab === 'transactions' || activeTab === 'system')) {
      setActiveTab('assets');
    }
  }, [user, activeTab]);

  if (!user) {
    return <Navigate to="/login" replace />;
  }

  return (
    <div className="min-h-screen bg-[#F9FAFB] flex flex-col lg:flex-row font-sans text-slate-800">
      <Sidebar 
        activeTab={activeTab} 
        setActiveTab={setActiveTab} 
        mobileOpen={mobileOpen} 
        setMobileOpen={setMobileOpen} 
        user={user}
        logout={logout}
      />
      
      <div className="flex-1 flex flex-col min-h-screen min-w-0 overflow-hidden">
        {/* Mobile Header — compact title only */}
        <header className="lg:hidden bg-white h-14 flex items-center justify-center px-4 sticky top-0 z-30 shadow-sm">
          <div className="flex items-center gap-2">
            <svg width="121" height="40" viewBox="0 0 121 40" fill="none" xmlns="http://www.w3.org/2000/svg" className="h-5 w-auto">
              <g clipPath="url(#clip0_2028_1728)">
                <path d="M0 -1.90735e-06C0 0.235198 0.956822 4.19083 7.35689 29.1862C7.8672 31.1747 8.59013 33.4411 8.97286 34.2109C10.2486 36.8622 12.9277 38.9363 15.9896 39.6632C17.9245 40.1123 29.1512 40.1123 30.8947 39.6632C33.4675 39.0004 35.4237 37.5037 37.1247 34.9592C37.5925 34.2536 39.74 30.4049 41.8875 26.4065C44.0351 22.4081 46.3314 18.1318 46.9906 16.8916C50.7541 9.96392 55.9209 0.128289 55.9209 -1.90735e-06H37.3729L30.8309 12.3373C27.2588 19.0512 24.282 24.5035 24.197 24.418C24.0481 24.247 23.4528 22.0019 20.7311 11.3324C19.6042 6.9277 18.4985 2.56581 18.2434 1.6464L17.8181 -1.90735e-06H8.90907C3.08309 -1.90735e-06 0 -1.90735e-06 0 -1.90735e-06ZM15.4792 5.2813C15.7344 6.20072 17.0314 11.3324 18.3922 16.6778C21.9218 30.4904 21.773 29.9345 22.3046 30.4904C22.8999 31.1105 24.1119 31.1747 24.8136 30.6187C25.2601 30.2552 26.5784 27.8391 36.4655 9.23694L39.4423 3.65629L44.6091 3.6349C48.3089 3.6349 49.7547 3.69905 49.7547 3.87011C49.7547 4.06254 45.3746 12.4014 40.2928 21.9163C39.4636 23.4558 38.5068 25.2519 36.0403 29.9131C33.9565 33.826 33.2761 34.7027 31.6389 35.6221L30.512 36.2421H23.4953H16.4786L15.2454 35.6221C13.9058 34.9379 12.8001 33.8474 12.141 32.5431C11.9071 32.0941 10.8015 28.1171 9.67453 23.7124C8.54761 19.3078 6.9529 13.1284 6.14492 9.96392C5.33694 6.82079 4.67779 4.1053 4.67779 3.93425C4.67779 3.67767 5.57083 3.6349 9.86589 3.6349H15.054L15.4792 5.2813Z" fill="#006675"/>
                <path d="M56.896 7.35668L53.1523 14.1578L55.5635 14.2219L57.9958 14.2861V19.0768C57.9958 22.1352 58.0804 23.8889 58.2073 23.8889C58.3342 23.8889 58.4188 23.8034 58.4188 23.7178C58.4188 23.5467 59.0322 22.3704 62.6489 15.6549C65.1658 10.9711 65.61 10.137 65.61 10.0515C65.61 10.0301 64.5101 9.9873 63.1777 9.9873H60.7454L60.7031 5.28213L60.6396 0.555582L56.896 7.35668Z" fill="#006675"/>
                <path d="M68.8313 12.5C68.7041 12.6922 67.9194 14.134 66.8802 16.0777C65.523 18.5767 64.4626 20.5631 60.1787 28.466L55.9796 36.2621L50.7202 36.3262C46.2242 36.3689 45.4819 36.3262 45.5667 36.0699C45.6304 35.899 47.7935 31.8621 50.3596 27.099C52.9469 22.3359 55.0518 18.4058 55.073 18.2777L51.1019 18.2777C50.9746 18.4272 50.402 19.3884 49.8507 20.4563C49.2993 21.5029 47.7511 24.3864 46.415 26.8641C45.079 29.3204 43.2975 32.5884 42.4917 34.1262C41.6646 35.6427 40.6254 37.6078 40.1588 38.4621L39.3105 40L48.7054 39.9573L58.0791 39.8932L59.6485 37.0097C60.518 35.4291 62.2146 32.2893 63.4022 30.068C64.611 27.8252 66.9863 23.4253 68.7041 20.2641C70.4219 17.1029 72.0549 14.0486 72.3518 13.4719L72.8077 12.5H68.8313Z" fill="#006675"/>
                <path d="M76.0723 14.5662C75.4341 15.7165 73.6473 18.9969 72.0944 21.8726C70.5416 24.7483 67.8613 29.6689 66.1383 32.8428C64.4153 35.9954 62.8412 38.9136 62.6497 39.2971L62.2881 40L69.9034 39.9361C78.0719 39.8509 78.2208 39.8296 80.6245 38.5941C81.7945 37.9764 83.7727 35.9314 84.5811 34.483C85.0916 33.567 85.3894 33.2049 85.4958 33.3966C85.5809 33.567 86.0063 35.1007 86.453 36.8261L87.2613 39.9787H96.6848H106.087L107.044 38.2107C107.576 37.2521 109.448 33.78 111.235 30.4996C113.022 27.2192 114.808 23.9175 115.213 23.1507C115.915 21.8726 116.744 20.3176 119.807 14.7366L120.977 12.6065L114.149 12.5426C109.81 12.5213 106.895 12.5852 106.172 12.7343C103.428 13.2881 100.812 15.2479 99.3863 17.7614C98.9822 18.507 98.578 19.1034 98.4929 19.1034C98.3866 19.1034 97.9398 17.6123 97.4931 15.8017L96.6635 12.5H86.9635H77.2423L76.0723 14.5662ZM94.983 20.5519C96.2168 25.4086 96.4934 25.9198 97.8548 25.9198C98.3015 25.9198 98.8333 25.7494 99.0673 25.5364C99.3013 25.3447 100.216 23.8323 101.109 22.1921C102.96 18.72 103.747 17.7188 105.3 16.8668L106.449 16.2277L110.66 16.1638C112.958 16.1212 114.851 16.1425 114.851 16.2064C114.851 16.2703 113.192 19.359 111.171 23.0868C109.15 26.8145 106.683 31.3304 105.725 33.0984L103.96 36.3575H97.1741C93.4515 36.3575 90.3883 36.2936 90.3883 36.1871C90.3883 36.1019 89.8565 33.993 89.2184 31.4795C87.9633 26.4524 87.6868 25.9198 86.3892 25.9198C85.3469 25.9198 84.7938 26.6015 82.8368 30.2866C80.901 33.9291 79.9651 35.0155 78.0931 35.761C77.0083 36.1871 76.4552 36.251 72.6475 36.3149C68.7122 36.3788 68.4357 36.3575 68.5633 36.0167C68.6909 35.6758 74.6684 24.5991 76.7743 20.8075C77.3273 19.8064 78.1357 18.3579 78.5611 17.5484L79.3482 16.1212L86.6019 16.1638L93.8556 16.2277L94.983 20.5519Z" fill="#006675"/>
                <path d="M76.6233 3.76018H74.0173C73.9989 3.54383 73.9496 3.34756 73.8695 3.17139C73.7925 2.99521 73.6847 2.84376 73.546 2.71704C73.4105 2.58722 73.2457 2.48832 73.0517 2.42032C72.8576 2.34923 72.6374 2.31369 72.3909 2.31369C71.9597 2.31369 71.5947 2.41877 71.2959 2.62895C71.0002 2.83912 70.7753 3.14048 70.6213 3.53301C70.4703 3.92554 70.3949 4.39689 70.3949 4.94705C70.3949 5.52812 70.4719 6.01492 70.6259 6.40746C70.783 6.7969 71.0094 7.09052 71.3051 7.28834C71.6008 7.48306 71.9566 7.58042 72.3724 7.58042C72.6096 7.58042 72.8222 7.55105 73.0101 7.49233C73.198 7.43051 73.3612 7.34243 73.4998 7.22807C73.6385 7.11371 73.7509 6.97616 73.8371 6.81544C73.9265 6.65163 73.9865 6.46773 74.0173 6.26373L76.6233 6.28228C76.5925 6.68408 76.4801 7.09362 76.286 7.51087C76.0919 7.92504 75.8162 8.3083 75.4589 8.66065C75.1047 9.00991 74.6657 9.29118 74.1421 9.50444C73.6184 9.71771 73.0101 9.82434 72.317 9.82434C71.4483 9.82434 70.669 9.63735 69.979 9.26336C69.2921 8.88937 68.7484 8.33921 68.348 7.61287C67.9506 6.88653 67.752 5.99793 67.752 4.94705C67.752 3.89 67.9553 2.99985 68.3619 2.2766C68.7685 1.55026 69.3168 1.00164 70.0068 0.630744C70.6967 0.256757 71.4668 0.0697632 72.317 0.0697632C72.9146 0.0697632 73.4644 0.151669 73.9665 0.315482C74.4686 0.479295 74.9091 0.718832 75.288 1.03409C75.6669 1.34627 75.9718 1.73107 76.2028 2.18851C76.4339 2.64595 76.574 3.16984 76.6233 3.76018Z" fill="#006675"/>
                <path d="M88.7774 4.94705C88.7774 6.00411 88.5726 6.8958 88.1629 7.62214C87.7532 8.34539 87.2003 8.89401 86.5042 9.268C85.808 9.63889 85.0318 9.82434 84.1754 9.82434C83.3129 9.82434 82.5336 9.63735 81.8375 9.26336C81.1444 8.88628 80.593 8.33612 80.1833 7.61287C79.7767 6.88653 79.5734 5.99793 79.5734 4.94705C79.5734 3.89 79.7767 2.99985 80.1833 2.2766C80.593 1.55026 81.1444 1.00164 81.8375 0.630744C82.5336 0.256757 83.3129 0.0697632 84.1754 0.0697632C85.0318 0.0697632 85.808 0.256757 86.5042 0.630744C87.2003 1.00164 87.7532 1.55026 88.1629 2.2766C88.5726 2.99985 88.7774 3.89 88.7774 4.94705ZM86.1345 4.94705C86.1345 4.37834 86.0591 3.89927 85.9081 3.50983C85.7603 3.1173 85.54 2.82058 85.2474 2.61968C84.9578 2.41568 84.6005 2.31369 84.1754 2.31369C83.7504 2.31369 83.3915 2.41568 83.0989 2.61968C82.8093 2.82058 82.5891 3.1173 82.4381 3.50983C82.2903 3.89927 82.2164 4.37834 82.2164 4.94705C82.2164 5.51576 82.2903 5.99638 82.4381 6.38891C82.5891 6.77835 82.8093 7.07507 83.0989 7.27906C83.3915 7.47997 83.7504 7.58042 84.1754 7.58042C84.6005 7.58042 84.9578 7.47997 85.2474 7.27906C85.54 7.07507 85.7603 6.77835 85.9081 6.38891C86.0591 5.99638 86.1345 5.51576 86.1345 4.94705Z" fill="#006675"/>
                <path d="M95.4286 9.69453H91.7876V0.199577H95.3916C96.365 0.199577 97.2059 0.389662 97.9144 0.769831C98.626 1.14691 99.1743 1.69089 99.5593 2.40178C99.9474 3.10957 100.141 3.95799 100.141 4.94705C100.141 5.93611 99.949 6.78608 99.5639 7.49697C99.1789 8.20476 98.6337 8.74874 97.9283 9.12891C97.2229 9.50599 96.3897 9.69453 95.4286 9.69453ZM94.3566 7.50624H95.3362C95.8044 7.50624 96.2033 7.43051 96.5329 7.27906C96.8656 7.12761 97.1182 6.86644 97.2907 6.49554C97.4662 6.12465 97.554 5.60848 97.554 4.94705C97.554 4.28562 97.4647 3.76946 97.286 3.39856C97.1105 3.02766 96.8517 2.76649 96.5098 2.61504C96.171 2.46359 95.7551 2.38787 95.2623 2.38787H94.3566V7.50624Z" fill="#006675"/>
                <path d="M103.152 9.69453V0.199577H109.972V2.2766H105.721V3.90854H109.62V5.98556H105.721V7.61751H109.953V9.69453H103.152Z" fill="#006675"/>
                <path d="M113.067 9.69453V0.199577H117.152C117.854 0.199577 118.469 0.327845 118.995 0.584382C119.522 0.840919 119.932 1.21027 120.224 1.69244C120.517 2.1746 120.663 2.75258 120.663 3.42638C120.663 4.10635 120.512 4.6797 120.21 5.14641C119.912 5.61312 119.491 5.96547 118.949 6.20346C118.41 6.44146 117.78 6.56045 117.059 6.56045H114.62V4.55761H116.542C116.844 4.55761 117.101 4.52052 117.313 4.44634C117.529 4.36907 117.694 4.24698 117.808 4.08008C117.925 3.91318 117.983 3.69528 117.983 3.42638C117.983 3.15439 117.925 2.93339 117.808 2.7634C117.694 2.59031 117.529 2.46359 117.313 2.38323C117.101 2.29978 116.844 2.25805 116.542 2.25805H115.636V9.69453H113.067ZM118.612 5.33649L120.978 9.69453H118.187L115.876 5.33649H118.612Z" fill="#006675"/>
              </g>
              <defs>
                <clipPath id="clip0_2028_1728">
                  <rect width="121" height="40" fill="white"/>
                </clipPath>
              </defs>
            </svg>
          </div>
        </header>

        {/* Main Content Area — bottom padding for mobile tab bar */}
        <main className="flex-1 p-4 sm:p-6 lg:p-8 pb-24 lg:pb-8 overflow-y-auto relative">
          {activeTab === 'economy' && <EconomyPage />}
          {activeTab === 'transactions' && <TransactionsPage />}
          {activeTab === 'assets' && <AssetsPage />}
          {activeTab === 'system' && <SystemPage />}
          <MqttDebugPanel />
        </main>

        {/* Mobile Bottom Tab Bar */}
        <MobileTabBar activeTab={activeTab} setActiveTab={setActiveTab} user={user} />
      </div>
    </div>
  );
}

export default function App() {
  const { user } = useAuth();
  return (
    <Routes>
      <Route path="/login" element={user ? <Navigate to="/app" replace /> : <Login />} />
      <Route path="/register" element={<Register />} />
      <Route path="/app" element={<DashboardLayout />} />
      <Route path="/" element={<Navigate to={user ? "/app" : "/login"} replace />} />
      <Route path="*" element={<Navigate to="/" replace />} />
    </Routes>
  );
}
