
 document.addEventListener("DOMContentLoaded", () => {
        


    document.getElementById("logout-button").addEventListener("click", () => {
        // Example: clear session and go to login
        localStorage.clear();
        window.location.href = "/login/login.html";
    });








    const panel = document.getElementById("control-menu");
    const button1 = document.querySelector(".control-button1");
    const button2 = document.querySelector(".control-button2");

    // Function to get the current control menu width from CSS variable
    function getMenuWidth() {
    return getComputedStyle(document.documentElement).getPropertyValue('--control-menu-width') || '25%';
    }

    function openPanel() {
        panel.style.width = getMenuWidth();
        button2.style.transform = "rotate(270deg)";
        button1.style.transform = "rotate(270deg)";
    }

    function closePanel() {
        panel.style.width = "0";
        button2.style.transform = "rotate(0deg)";
        button1.style.transform = "rotate(0deg)"; // rotate back when closing
    }

    button1.addEventListener("click", (e) => {
        e.stopPropagation();
        if (panel.style.width === getMenuWidth()) {
            closePanel();
        } else {
            openPanel();
        }
    });

    // Toggle panel when button2 clicked
    button2.addEventListener("click", (e) => {
        e.stopPropagation();
        closePanel();
    });


    document.addEventListener("click", (e) => {
        if (!panel.contains(e.target) && panel.style.width === getMenuWidth()) {
            panel.style.width = "0";
            button2.style.transform = "rotate(0deg)";
            button1.style.transform = "rotate(0deg)";
        }
    });



        let lastDHTSave = 0;
        async function updateDHT() {
            try {
                const response = await fetch('/sensor-data');
                const data = await response.json();
                
                document.getElementById('temp-value').innerText = (data.temperature !== undefined ? data.temperature.toFixed(2) : '--') + ' C';
    

                document.getElementById('humid-value').innerText = (data.humidity !== undefined ? data.humidity.toFixed(2) : '--') + ' %';
                const now = Date.now();
                if (now - lastDHTSave >= 10000) {
                    pushValue('temperature', data.temperature);
                    pushValue('humidity', data.humidity);
                    lastDHTSave = now;

                    if (chart) chart.update();
                }
                
            } catch (error) {
                console.error('Error fetching sensor data:', error);
            }
        }

        async function updateSound() {
            try {
                const response = await fetch('/sound-data');
                const data = await response.json();

                document.getElementById('sound-value').innerText = (data.sound !== undefined ? data.sound : '--');
                document.getElementById('soundSenStatus').innerText = data.clapDetected || '--';
                pushValue('sound', Number(data.sound)); //for graph
                if (chart) chart.update();



            }
            catch (error) {
                console.error('Sound fetch error: ', error);

            }
        }
            setInterval(updateDHT, 2000); // Update every 2 seconds
            updateDHT();
            setInterval(updateSound, 200); // Update every 200 milliseconds
            updateSound();


        
    




    async function updateLDR() {
        try {
        const response = await fetch('/ldr-data'); // call ESP32 endpoint
        const data = await response.json();

        // Update the <p id="light-value">
        document.getElementById('light-value').innerText = 
        (data.light !== undefined ? data.light : '--') + ' Lx';
        pushValue('light', data.light);
        if (chart) chart.update();



        } 
        catch (error) {
            console.error('Error fetching LDR data:', error);
        }
    
    }
        updateLDR();
        setInterval(updateLDR, 200);






    async function updateLEDStates() {
    try {
        const response = await fetch('/led-status');
        const data = await response.json();

        // Get table cells
        const room1Cell = document.getElementById('room1State');
        const room2Cell = document.getElementById('room2State');

        // Update text
        room1Cell.innerText = data.ldr || '--';
        room2Cell.innerText = data.sound || '--';

        // Set color based on state
        room1Cell.style.color = data.ldr === 'ON' ? 'green' : 'red';
        room2Cell.style.color = data.sound === 'ON' ? 'green' : 'red';

        // Update checkboxes
        document.getElementById('room1Control').checked = data.ldr === 'ON';
        document.getElementById('room2Control').checked = data.sound === 'ON';

    } catch (error) {
        console.error('Error fetching LED states:', error);
    }
}
        setInterval(updateLEDStates, 200);
        updateLEDStates();




document.getElementById('room1Control').addEventListener('change', async () => {
    let state = document.getElementById('room1Control').checked ? 'on' : 'off';
    await fetch(`/room1?state=${state}`);
});

document.getElementById('room2Control').addEventListener('change', async () => {
    let state = document.getElementById('room2Control').checked ? 'on' : 'off';
    await fetch(`/room2?state=${state}`);
});





    const doorControl = document.getElementById('door-control');
    //const doorStatus = document.getElementById('door-status');
    
    doorControl.addEventListener('change', async () => {
        let state = doorControl.checked ? 'open' : 'close';
        try {
            const response = await fetch('/door?state=' + state);
            if (!response.ok) throw new Error('Network response not ok');
            console.log('Door command sent: ', state);
        } catch (error) {
            console.error('Error sending door command:', error);


        }
        //doorStatus.innerText = state === 'open' ? 'OPEN' : 'CLOSED';
    });
    //doorStatus.innerText = doorControl.checked ? 'OPEN' : 'CLOSED';





    const doorStatus = document.getElementById('door-status');

    async function updateDoorStatus() {
        try {
            const response = await fetch('/door-status');
            const data = await response.json();
            doorStatus.innerText = data.door;
            doorStatus.style.color = data.door === 'OPEN' ? 'green' : 'red';
        } catch (error) {
            console.error('Error fetching door status:', error);
            //doorStatus.innerText = '--';
        }
        
    }

    // Update every 500ms

    setInterval(updateDoorStatus, 500);
    updateDoorStatus();
    






    const boxes = document.querySelectorAll('.box');
    const modal = document.getElementById('sensor-modal');
    const closeModal = document.getElementById('close-modal');
    const modalTitle = document.getElementById('modal-title');

    boxes.forEach(box => {
        box.addEventListener('click', () => {
            const sensor = box.getAttribute('sensor-data');
            modalTitle.innerText = sensor.charAt(0).toUpperCase() + sensor.slice(1) + " Data";
            modal.classList.remove('hidden');
            createChart(sensor);
        });
    });

    closeModal.addEventListener('click', () => {
        modal.classList.add('hidden');
    });

    //close modal when clicking outside content
    modal.addEventListener('click', (e) => {
        if(e.target === modal) modal.classList.add('hidden');
    });



    const history = {
        temperature: [],
        humidity: [],
        light: [],
        sound: []
    };

    const MAX_POINTS = {
        temperature: 30, 
        humidity: 30,
        light: 50,
        sound: 50
    };

    function pushValue(sensor, value) {
        if (value === undefined || isNaN(value)) return;

        history[sensor].push({
            x: new Date(),
            y: value
        });

        if (history[sensor].length > MAX_POINTS[sensor]) {
            history[sensor].shift();
        }
    }

    



    let chart;

    function createChart(sensor) {
        const ctx = document.getElementById('sensorChart').getContext('2d');

        if (chart) chart.destroy();
        const isDarkMode = document.body.classList.contains('dark-mode');

        chart = new Chart(ctx, {
            type: 'line',
            data: {
                datasets: [{
                    label: sensor.toUpperCase(),
                    data: history[sensor],
                    borderWidth: 2,
                    tension: 0.3,
                    pointRadius: 2,
                    fill: false,
                    borderColor: isDarkMode ? 'white' : 'black', // line color
                    backgroundColor: isDarkMode ? 'white' : 'black', // point color
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                plugins: {
                    legend: {
                        labels: {
                            color: isDarkMode ? 'white' : 'black' // legend text color
                        }
                    }
                },
                scales: {
                    x: {
                        type: 'time',
                        time: {
                            unit: sensor === 'temperature' || sensor === 'humidity' ? 'minute' : 'second'
                        },
                        ticks: {
                            color: isDarkMode ? 'white' : 'black' // x-axis labels
                        },
                        grid: {
                            color: isDarkMode ? 'rgba(255,255,255,0.2)' : 'rgba(0,0,0,0.1)' // grid lines
                        }
                    },


                    y: {
                        beginAtZero: true,
                        ticks: {
                            color: isDarkMode ? 'white' : 'black' // y-axis labels
                        },
                        grid: {
                            drawTicks: true,
                            color: isDarkMode ? 'rgba(255,255,255,0.2)' : 'rgba(0,0,0,0.1)' // grid lines
                        }
                    }

                }
            }
        });
    }




    const toggle = document.getElementById("darkModeToggle");


    if (localStorage.getItem("theme") === "dark") {
        document.body.classList.add("dark-mode");
        toggle.checked = true;
    }

    toggle.addEventListener("change", () => {
        if (toggle.checked) {
            document.body.classList.add("dark-mode");
            localStorage.setItem("theme", "dark");
        } else {
            document.body.classList.remove("dark-mode");
            localStorage.setItem("theme", "light");
        }
    });



let lastAlarmTime = 0;

async function checkAlarm() {
    try {
        const res = await fetch('/alarm-status');
        const data = await res.json();

        if (data.alarm && data.time !== lastAlarmTime) {
            lastAlarmTime = data.time;
            showAlert();
        }
    } catch (err) {
        console.error("Alarm check failed", err);
    }
}

function showAlert() {
    const alertBox = document.getElementById('alert-notification');
    const bar = alertBox.querySelector('.alert-bar');

    // reset animation
    bar.style.animation = 'none';       
    bar.offsetHeight;
    bar.style.animation = 'alertProgress 5s linear forwards';

    alertBox.classList.add('show');

    setTimeout(() => {
        alertBox.classList.remove('show');
    }, 5000);
}
setInterval(checkAlarm, 500);


    const alarmCheckbox = document.getElementById('trigger-alarm');

    alarmCheckbox.addEventListener('change', async () => {
        const state = alarmCheckbox.checked ? 'on' : 'off';
        try {
            await fetch(`/alarm-toggle?state=${state}`);
        } catch (err) {
            console.error("Failed to toggle alarm:", err);
        }
    });

    async function updateBuzzerStatus() {
        try {
            const response = await fetch('/buzzer-status');
            const data = await response.json();
            document.getElementById('buzzer-status').innerText = data.status;
            document.getElementById('buzzer-status').style.color = (data.status === "Active") ? "green" : "red";
        } catch (error) {
            console.error('Error fetching buzzer status:', error);
            document.getElementById('buzzer-status').innerText = "--";
            document.getElementById('buzzer-status').style.color = "black";
        }
    }

    // Update every 200ms
    setInterval(updateBuzzerStatus, 200);
    updateBuzzerStatus();

    async function updateUltrasonicStatus() {
        try {
            const response = await fetch('/ultrasonic-data');
            const data = await response.json();

            const statusCell = document.getElementById('ultrasonic-status');
            statusCell.innerText = data.status; // display "Object Detected!" or "No Object"
            statusCell.style.color = (data.status === "Object Detected!") ? "green" : "red";
        } catch (error) {
            console.error('Error fetching ultrasonic status:', error);
            const statusCell = document.getElementById('ultrasonic-status');
            //statusCell.innerText = "--";
            //statusCell.style.color = "black";
        }
    }

    // Update every 200ms
    setInterval(updateUltrasonicStatus, 200);
    updateUltrasonicStatus();

    let authorizedUntil = 0;

    async function updateTouchStatus() {
        try {
            const res = await fetch('/touch-status');
            const data = await res.json();

            const cell = document.getElementById('capacitive-status');
            const now = Date.now();

            // If authorized event just happened → start 10s window
            if (data.authorized) {
                authorizedUntil = now + 9500; //timer 
            }

            // Show AUTHORIZED while timer is active
            if (now < authorizedUntil) {
                cell.innerText = "Access Granted!";
                cell.style.color = "green";
                return;
            }

            // After timeout, reset display
            if (data.touch1 && data.touch2) {
                cell.innerText = "Authenticating...";
                cell.style.color = "orange";
            } 
            else if (data.touch1 || data.touch2) {
                cell.innerText = "Awaiting Dual Touch...";
                cell.style.color = "orange";
            } 
            else {
                cell.innerText = "Idle";
                cell.style.color = "red";
            }

        } catch (err) {
            console.error("Touch status fetch failed:", err);
        }
    }

    setInterval(updateTouchStatus, 200);
    updateTouchStatus();




    async function updateActivityLog() {
        try {
            const res = await fetch('/activity-log');
            const activities = await res.json();

            const container = document.querySelector('.activity-log');
            if (!container) return;

            container.innerHTML = '<h3>Recent Activity</h3>'; // reset header

            activities.forEach(act => {
                const div = document.createElement('div');
                div.className = 'activity-item';

                const span = document.createElement('span');
                span.innerText = act.message;

                const small = document.createElement('small');
                // Convert elapsed time to human-readable
                const mins = Math.floor(act.elapsed / 60);
                const secs = act.elapsed % 60;
                small.innerText = mins > 0 ? `${mins} mins ago` : `${secs} secs ago`;

                div.appendChild(span);
                div.appendChild(small);
                container.appendChild(div);
            });

        } catch (err) {
            console.error('Failed to update activity log', err);
        }
    }

    // Initial fetch and update every 5s
    updateActivityLog();
    setInterval(updateActivityLog, 5000);


    //Email JS
    document.getElementById("contact-form").addEventListener("submit", function(e) {
        e.preventDefault();

        emailjs.sendForm(
            "service_a15stym",   // from EmailJS
            "template_ol8i408",  // from EmailJS
            this
        ).then(() => {
            alert("Message sent successfully!");
            this.reset();
        }, (error) => {
            alert("Failed to send message.");
            console.error(error);
        });
    });





});