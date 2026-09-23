#ifndef WEBPAGES_H
#define WEBPAGES_H
#include <Arduino.h>

// ==========================================
// OTA & DASHBOARD PAGE (Normal Boot)
// ==========================================
const char dashboardHTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Dashboard - Control Panel</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, sans-serif; background-color: #1e1e1e; color: #e0e0e0; padding: 20px; display: flex; justify-content: center; margin: 0; }
    .card { background: #2d2d2d; padding: 30px; border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.5); max-width: 400px; width: 100%; margin-bottom: 20px; }
    h2 { color: #5cd65c; margin-top: 0; }
    h3 { color: #4da6ff; margin-bottom: 10px; border-bottom: 1px solid #444; padding-bottom: 5px; }
    .toggle-group { display: flex; flex-direction: column; gap: 10px; margin-bottom: 20px; }
    input[type=submit] { background-color: #4da6ff; color: #121212; padding: 12px; border: none; width: 100%; font-weight: bold; cursor: pointer; border-radius: 6px; }
    input[type=file] { margin: 15px 0; color: #ccc; width: 100%; }
    select { width: 100%; padding: 10px; margin-bottom: 15px; background: #1a1a1a; color: white; border: 1px solid #444; border-radius: 6px; }
    .btn-green { background-color: #5cd65c !important; }
  </style>
</head>
<body>
  <div>
    <!-- Settings Form -->
    <div class="card">
      <h2>Control Panel</h2>
      <form action='/updateSettings' method='POST'>
        
        <h3>Football Settings</h3>
        <select id="leagueSelect" name="league">
          <option value="">-- Select League --</option>
          <option value="4328">Premier League</option>
          <option value="4329">Championship</option>
          <option value="4396">League One</option>
          <option value="4397">League Two</option>
          <option value="4590">National League</option>
        </select>
        <select id="teamSelect" name="team">
          <option value="">-- Select Team --</option>
        </select>

        <h3>Enable/Disable Apps</h3>
        <div class="toggle-group">
          <label><input type="checkbox" name="en_fish" value="1"> Fish Tank</label>
          <label><input type="checkbox" name="en_led" value="1"> LED Controller</label>
          <label><input type="checkbox" name="en_wea" value="1"> Weather</label>
          <label><input type="checkbox" name="en_ftb" value="1"> Football</label>
          <label><input type="checkbox" name="en_sys" value="1"> Sys Monitor</label>
          <label><input type="checkbox" name="en_sum" value="1"> Global Summary</label>
        </div>
        <input type='submit' value='Save Settings & Reboot'>
      </form>
    </div>

    <!-- OTA Form -->
    <div class="card">
      <h3 style="color:#5cd65c;">Firmware Update</h3>
      <form method='POST' action='/update' enctype='multipart/form-data'>
        <input type='file' name='update' accept='.bin' required>
        <input type='submit' class="btn-green" value='Upload & Flash'>
      </form>
    </div>
  </div>

  <script>
    const leagueSelect = document.getElementById('leagueSelect');
    const teamSelect = document.getElementById('teamSelect');

    leagueSelect.addEventListener('change', async function() {
      const selectedLeague = this.value;
      
      if (!selectedLeague) {
        teamSelect.innerHTML = '<option value="">-- Select Team --</option>';
        return;
      }

      teamSelect.innerHTML = '<option value="">-- Loading Teams... --</option>';
      
      try {
        const response = await fetch(`https://www.thesportsdb.com/api/v1/json/1/lookuptable.php?l=${selectedLeague}`);
        const data = await response.json();
        
        teamSelect.innerHTML = '<option value="">-- Select Team --</option>';
        
        if (data.table) {
          // Sort alphabetically
          data.table.sort((a, b) => a.strTeam.localeCompare(b.strTeam));
          
          data.table.forEach(team => {
            const option = document.createElement('option');
            option.value = team.strTeam;
            option.textContent = team.strTeam;
            teamSelect.appendChild(option);
          });
        } else {
          teamSelect.innerHTML = '<option value="">-- No Teams Found --</option>';
        }
      } catch (error) {
        teamSelect.innerHTML = '<option value="">-- Error Loading Teams --</option>';
      }
    });
  </script>
</body>
</html>
)=====";

// ==========================================
// FIRST BOOT SETUP PAGE (AP Mode)
// ==========================================
const char setupHTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Dashboard - Setup</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, sans-serif; background-color: #1e1e1e; color: #e0e0e0; padding: 20px; display: flex; justify-content: center; margin: 0; }
    .card { background: #2d2d2d; padding: 30px; border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.5); max-width: 400px; width: 100%; margin-bottom: 20px; }
    h2, h3 { color: #4da6ff; margin-bottom: 10px; }
    p { font-size: 14px; color: #aaaaaa; margin-bottom: 20px; text-align: center; }
    input[type=password], input[type=text], select { width: 100%; padding: 12px; margin: 5px 0 20px 0; box-sizing: border-box; border: 1px solid #444; background-color: #1a1a1a; color: white; border-radius: 6px; }
    .toggle-group { display: flex; flex-direction: column; gap: 10px; margin-bottom: 25px; }
    input[type=submit] { background-color: #4da6ff; color: #121212; padding: 15px; border: none; width: 100%; font-weight: bold; font-size: 16px; cursor: pointer; border-radius: 6px; }
    input[type=file] { margin: 15px 0; color: #ccc; width: 100%; }
    .btn-green { background-color: #5cd65c !important; }
  </style>
</head>
<body>
  <div>
    <!-- Settings Form -->
    <div class="card">
      <h2 style="text-align: center;">System Setup</h2>
      <p>Configure your smart dashboard settings below.</p>
      
      <form action='/save' method='POST'>
        
        <h3>Network</h3>
        <input type='text' name='ssid' placeholder='Wi-Fi Name (SSID)' required>
        <input type='password' name='pass' placeholder='Wi-Fi Password'>

        <h3>Security</h3>
        <input type='text' name='devName' value='CYD-Dashboard' required>
        <input type='password' name='pwd' placeholder='New Admin Password' required minlength='4'>

        <h3>Football</h3>
        <select id="leagueSelect" name="league">
          <option value="">-- Select League --</option>
          <option value="4328">Premier League</option>
          <option value="4329">Championship</option>
          <option value="4396">League One</option>
          <option value="4397">League Two</option>
          <option value="4590">National League</option>
        </select>
        <select id="teamSelect" name="team">
          <option value="">-- Select Team --</option>
        </select>

        <h3>Enable Apps</h3>
        <div class="toggle-group">
          <label><input type="checkbox" name="en_fish" checked> Fish Tank</label>
          <label><input type="checkbox" name="en_led" checked> LED Controller</label>
          <label><input type="checkbox" name="en_wea" checked> Weather</label>
          <label><input type="checkbox" name="en_ftb" checked> Football</label>
          <label><input type="checkbox" name="en_sys" checked> Sys Monitor</label>
          <label><input type="checkbox" name="en_sum" checked> Global Summary</label>
        </div>

        <input type='submit' value='Save & Reboot'>
      </form>
    </div>

    <!-- OTA Form -->
    <div class="card">
      <h3 style="color:#5cd65c;">Emergency Firmware Update</h3>
      <p style="text-align: left; margin-top: 0;">Use this if the board is stuck in Setup Mode.</p>
      <form method='POST' action='/update' enctype='multipart/form-data'>
        <input type='file' name='update' accept='.bin' required>
        <input type='submit' class="btn-green" value='Upload & Flash'>
      </form>
    </div>
  </div>

  <script>
    const leagueSelect = document.getElementById('leagueSelect');
    const teamSelect = document.getElementById('teamSelect');

    leagueSelect.addEventListener('change', async function() {
      const selectedLeague = this.value;
      
      if (!selectedLeague) {
        teamSelect.innerHTML = '<option value="">-- Select Team --</option>';
        return;
      }

      teamSelect.innerHTML = '<option value="">-- Loading Teams... --</option>';
      
      try {
        const response = await fetch(`https://www.thesportsdb.com/api/v1/json/1/lookuptable.php?l=${selectedLeague}`);
        const data = await response.json();
        
        teamSelect.innerHTML = '<option value="">-- Select Team --</option>';
        
        if (data.table) {
          // Sort alphabetically
          data.table.sort((a, b) => a.strTeam.localeCompare(b.strTeam));
          
          data.table.forEach(team => {
            const option = document.createElement('option');
            option.value = team.strTeam;
            option.textContent = team.strTeam;
            teamSelect.appendChild(option);
          });
        } else {
          teamSelect.innerHTML = '<option value="">-- No Teams Found --</option>';
        }
      } catch (error) {
        teamSelect.innerHTML = '<option value="">-- Error Loading Teams --</option>';
      }
    });
  </script>
</body>
</html>
)=====";

#endif