// Firebase configuration
const firebaseConfig = {
  apiKey: "AIzaSyCTTvBU_3Y5ABRrtOQsZCfgoSenNnYJ1kA",
  authDomain: "smart-home-bebd9.firebaseapp.com",
  databaseURL: "https://smart-home-bebd9-default-rtdb.firebaseio.com",
  projectId: "smart-home-bebd9",
  storageBucket: "smart-home-bebd9.firebasestorage.app",
  messagingSenderId: "967301332630",
  appId: "1:967301332630:web:4785c6c131d669a1bc0cb7",
  measurementId: "G-WCFWZ49V2H"
};

// Initialize Firebase
firebase.initializeApp(firebaseConfig);
const database = firebase.database();

// Elements
const elements = {
  temp: document.getElementById('nhietdo'),
  humi: document.getElementById('doam'),
  co2: document.getElementById('khico2'),
  denImg: document.getElementById('imgDen'),
  quatImg: document.getElementById('imgQuat'),
  loaImg: document.getElementById('imgLoa'),
  logList: document.getElementById('log-list'),
  time: document.getElementById('time'),
  menuToggle: document.getElementById('menu-toggle'),
  sidebar: document.getElementById('sidebar'),
  main: document.getElementById('main'),
  dynamicContent: document.getElementById('dynamicContent'),
  bieudoContent: document.getElementById('bieudo-content')
};

const buttons = {
  denOn: document.getElementById('btnDenOn'),
  denOff: document.getElementById('btnDenOff'),
  quatOn: document.getElementById('btnQuatOn'),
  quatOff: document.getElementById('btnQuatOff'),
  loaOn: document.getElementById('btnLoaOn'),
  loaOff: document.getElementById('btnLoaOff')
};

// Variables
let chartInstance = null;
let currentRoom = 'phong1';
let logs = {};
let sensorData = {};

// Check Firebase connection
database.ref('.info/connected').on('value', snap => {
  if (snap.val() === true) {
    console.log('Đã kết nối với Firebase');
  } else {
    console.log('Mất kết nối với Firebase');
  }
});

// Add log entry
function addLogEntry(message) {
  const now = new Date();
  const timeStr = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`;
  const logMessage = `[${timeStr}] ${message} - Phòng: ${currentRoom}`;

  // Chỉ đẩy log lên Firebase, không thêm trực tiếp vào logs[currentRoom]
  const logRef = database.ref(`${currentRoom}/logs`).push();
  logRef.set({ message: logMessage })
    .catch(error => console.error(`Lỗi thêm nhật ký cho ${currentRoom}:`, error));
}

// Load logs
function loadLogs() {
  const rooms = ['phong1', 'phong2', 'phong3', 'phong4'];
  rooms.forEach(room => {
    database.ref(`${room}/logs`).on('child_added', snap => {
      const log = snap.val();
      if (!logs[room]) logs[room] = [];
      logs[room].unshift(log.message);
      if (logs[room].length > 10) {
        logs[room].pop();
      }
      if (currentRoom === room && elements.main.style.display !== 'none') {
        displayLogs();
      }
    }, error => console.error(`Lỗi đọc nhật ký cho ${room}:`, error));
  });
}

// Display logs
function displayLogs() {
  elements.logList.innerHTML = '';
  const roomLogs = logs[currentRoom] || [];
  roomLogs.forEach(log => {
    const li = document.createElement('li');
    li.textContent = log;
    elements.logList.prepend(li);
  });
  while (elements.logList.children.length > 10) {
    elements.logList.removeChild(elements.logList.lastChild);
  }
}

// Initialize sensor data and logs
function initializeData() {
  const rooms = ['phong1', 'phong2', 'phong3', 'phong4'];
  rooms.forEach(room => {
    sensorData[room] = {
      temp: [],
      humi: [],
      co2: [],
      timestamps: []
    };

    database.ref(room).on('value', snap => {
      try {
        const data = snap.val() || { 
          temp: 0, 
          humi: 0, 
          co2: 0, 
          den: 0, 
          quat: 0, 
          loa: 0, 
          totalTime: { den: 0, quat: 0, loa: 0 }, 
          status: { connection: 0 } 
        };
        const now = new Date();
        const timeStr = now.toLocaleTimeString();

        sensorData[room].timestamps.push(timeStr);
        sensorData[room].temp.push(data.temp || 0);
        sensorData[room].humi.push(data.humi || 0);
        sensorData[room].co2.push(data.co2 || 0);

        if (sensorData[room].timestamps.length > 10) {
          sensorData[room].timestamps.shift();
          sensorData[room].temp.shift();
          sensorData[room].humi.shift();
          sensorData[room].co2.shift();
        }

        if (currentRoom === room) {
          updateDisplay(data);
          updateDeviceStates(data);
          if (elements.bieudoContent.style.display === 'block') {
            renderChart();
          }
        }
      } catch (error) {
        console.error(`Lỗi xử lý dữ liệu cho ${room}:`, error);
      }
    }, error => console.error(`Lỗi lắng nghe ${room}:`, error));

    database.ref(room).once('value', snap => {
      if (!snap.exists()) {
        const defaultData = {
          temp: 0,
          humi: 0,
          co2: 0,
          den: 0,
          quat: 0,
          loa: 0,
          status: { connection: 0 },
          totalTime: { den: 0, quat: 0, loa: 0 }
        };
        database.ref(room).set(defaultData)
          .then(() => console.log(`Khởi tạo dữ liệu mặc định cho ${room}`))
          .catch(error => console.error(`Lỗi khởi tạo ${room}:`, error));
      } else {
        const data = snap.val();
        if (data.den === undefined) database.ref(room + '/den').set(0);
        if (data.quat === undefined) database.ref(room + '/quat').set(0);
        if (data.loa === undefined) database.ref(room + '/loa').set(0);
        if (data.status === undefined || data.status.connection === undefined) {
          database.ref(room + '/status/connection').set(0);
        }
        if (data.totalTime === undefined) {
          database.ref(room + '/totalTime').set({ den: 0, quat: 0, loa: 0 });
        }
      }
    }, error => console.error(`Lỗi kiểm tra sự tồn tại của ${room}:`, error));
  });
}

// Update display
function updateDisplay(data) {
  elements.temp.innerText = `${isNaN(data.temp) ? 0 : data.temp.toFixed(1)} °C`;
  elements.humi.innerText = `${isNaN(data.humi) ? 0 : data.humi.toFixed(1)} %`;
  elements.co2.innerText = `${isNaN(data.co2) ? 0 : data.co2.toFixed(0)} ppm`;
}

// Update device states
function updateDeviceStates(data) {
  elements.denImg.src = data.den ? 'image/densang.gif' : 'image/dentat.png';
  elements.quatImg.src = data.quat ? 'image/quatbat.gif' : 'image/quattat.png';
  elements.loaImg.src = data.loa ? 'image/loabat.gif' : 'image/loatat.png';
}

// Toggle device
function toggleDevice(device, state) {
  console.log(`Cập nhật ${currentRoom}/${device} thành ${state}`);
  const ref = database.ref(currentRoom).child(device);
  ref.set(state).then(() => {
    console.log(`Thành công: ${currentRoom}/${device} = ${state}`);
    if (device === 'den') {
      elements.denImg.src = state ? 'image/densang.gif' : 'image/dentat.png';
      addLogEntry(`Đèn được ${state ? 'bật' : 'tắt'}`);
    } else if (device === 'quat') {
      elements.quatImg.src = state ? 'image/quatbat.gif' : 'image/quattat.png';
      addLogEntry(`Quạt được ${state ? 'bật' : 'tắt'}`);
    } else if (device === 'loa') {
      elements.loaImg.src = state ? 'image/loabat.gif' : 'image/loatat.png';
      addLogEntry(`Loa được ${state ? 'bật' : 'tắt'}`);
    }
  }).catch(error => {
    console.error(`Lỗi điều khiển ${device} cho ${currentRoom}:`, error);
  });
}

// Event listeners
function setupEventListeners() {
  buttons.denOn.addEventListener('click', () => toggleDevice('den', 1));
  buttons.denOff.addEventListener('click', () => toggleDevice('den', 0));
  buttons.quatOn.addEventListener('click', () => toggleDevice('quat', 1));
  buttons.quatOff.addEventListener('click', () => toggleDevice('quat', 0));
  buttons.loaOn.addEventListener('click', () => toggleDevice('loa', 1));
  buttons.loaOff.addEventListener('click', () => toggleDevice('loa', 0));

  elements.menuToggle.addEventListener('click', () => {
    elements.sidebar.classList.toggle('open');
  });
}

// Select room
function selectRoom(room) {
  if (!['phong1', 'phong2', 'phong3', 'phong4'].includes(room)) {
    console.error(`Phòng không hợp lệ: ${room}`);
    return;
  }
  currentRoom = room;
  console.log(`Chuyển sang phòng: ${currentRoom}`);

   const backgroundImages = {
    phong1: 'url("image/phong1.avif")', 
    phong2: 'url("image/phong2.avif")', 
    phong3: 'url("image/phong3.avif")', 
    phong4: 'url("image/phong4.avif")'
  };
  document.body.style.backgroundImage = backgroundImages[room] || 'linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%)';

  database.ref(currentRoom).once('value', snap => {
    const data = snap.val() || { 
      temp: 0, 
      humi: 0, 
      co2: 0, 
      den: 0, 
      quat: 0, 
      loa: 0, 
      totalTime: { den: 0, quat: 0, loa: 0 }, 
      status: { connection: 0 } 
    };
    console.log(`Dữ liệu cho ${currentRoom}:`, data);
    updateDisplay(data);
    updateDeviceStates(data);
    displayLogs();
    if (elements.bieudoContent.style.display === 'block') {
      renderChart();
    }
  }, error => console.error(`Lỗi lấy dữ liệu cho ${currentRoom}:`, error));
}

// Show section
function showSection(section) {
  if (section === 'home') {
    elements.main.style.display = 'block';
    elements.dynamicContent.style.display = 'none';
    displayLogs();
  } else if (section === 'bieudo') {
    elements.main.style.display = 'none';
    elements.dynamicContent.style.display = 'block';
    elements.bieudoContent.style.display = 'block';
    renderChart();
  }
  elements.sidebar.classList.remove('open');
}

// Charts
const ctx = document.getElementById('humidityChart').getContext('2d');
function renderChart() {
  if (chartInstance) {
    chartInstance.destroy();
  }

  chartInstance = new Chart(ctx, {
    type: 'bar',
    data: {
      labels: sensorData[currentRoom].timestamps,
      datasets: [
        {
          label: 'Nhiệt độ (°C)',
          data: sensorData[currentRoom].temp,
          backgroundColor: 'rgba(255, 99, 132, 0.6)',
          borderColor: 'rgba(255, 99, 132, 1)',
          borderWidth: 1,
          barThickness: 20,
        },
        {
          label: 'Độ ẩm (%)',
          data: sensorData[currentRoom].humi,
          backgroundColor: 'rgba(54, 162, 235, 0.6)',
          borderColor: 'rgba(54, 162, 235, 1)',
          borderWidth: 1,
          barThickness: 20,
        },
        {
          label: 'Khí CO2 (ppm)',
          data: sensorData[currentRoom].co2,
          backgroundColor: 'rgba(255, 165, 0, 0.6)',
          borderColor: 'rgba(255, 165, 0, 1)',
          borderWidth: 1,
          barThickness: 20,
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: {
        y: {
          beginAtZero: true,
          title: { display: true, text: 'Giá trị' },
          ticks: {
            stepSize: 100
          }
        },
        x: {
          title: { display: true, text: 'Thời gian' }
        }
      },
      plugins: {
        legend: { position: 'top' },
        title: { display: true, text: `Dữ liệu ${currentRoom}` }
      }
    }
  });
}

// Clock
function updateClock() {
  const now = new Date();
  let hours = now.getHours().toString().padStart(2, '0');
  let minutes = now.getMinutes().toString().padStart(2, '0');
  let seconds = now.getSeconds().toString().padStart(2, '0');
  let timeString = `${hours}:${minutes}:${seconds}`;
  elements.time.textContent = timeString;
}

// Initialize
window.onload = () => {
  initializeData();
  loadLogs();
  setupEventListeners();
  setInterval(updateClock, 1000);
  updateClock();
};