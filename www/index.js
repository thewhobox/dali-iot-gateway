let socket;
let offsetTimestamp = Date.now() / 1000;
let lastTimestamp = null;
let lastExtended = -1;
let reconnectAttempts = 0;
const maxLogEntries = 500;

// Command definitions
const commandMap = {
    0: "OFF",
    1: "UP",
    2: "DOWN",
    3: "STEP UP",
    4: "STEP DOWN",
    5: "RECALL MAX",
    6: "RECALL MIN",
    7: "STEP DOWN AND OFF",
    8: "ON AND STEP UP",
    10: "GO TO LAST", // DALI-2
    16: "GO TO SCENE 0",
    17: "GO TO SCENE 1", 
    18: "GO TO SCENE 2", 
    19: "GO TO SCENE 3", 
    20: "GO TO SCENE 4", 
    21: "GO TO SCENE 5", 
    22: "GO TO SCENE 6", 
    23: "GO TO SCENE 7",
    24: "GO TO SCENE 8",
    25: "GO TO SCENE 9",
    26: "GO TO SCENE 10",
    27: "GO TO SCENE 11",
    28: "GO TO SCENE 12",
    29: "GO TO SCENE 13",
    30: "GO TO SCENE 14",
    31: "GO TO SCENE 15",
    32: "DEVICE RESET",
    33: "ARC TO_DTR",
    34: "SAVE VARS",
    35: "SET OPMODE",
    36: "RESET MEM",
    37: "IDENTIFY", // DALI-2
    42: "DTR AS MAX",
    43: "DTR AS MIN",
    44: "DTR AS FAIL",
    45: "DTR AS POWER ON",
    46: "DTR AS FADE TIME",
    47: "DTR AS FADE RATE",
    48: "DTR AS EXT FADE TIME", // DALI-2
    64: "DTR AS SCENE 0",
    65: "DTR AS SCENE 1",
    66: "DTR AS SCENE 2",
    67: "DTR AS SCENE 3",
    68: "DTR AS SCENE 4",
    69: "DTR AS SCENE 5",
    70: "DTR AS SCENE 6",
    71: "DTR AS SCENE 7",
    72: "DTR AS SCENE 8",
    73: "DTR AS SCENE 9",
    74: "DTR AS SCENE 10",
    75: "DTR AS SCENE 11",
    76: "DTR AS SCENE 12",
    77: "DTR AS SCENE 13",
    78: "DTR AS SCENE 14",
    79: "DTR AS SCENE 15",
    80: "REMOVE FROM SCENE 0",
    81: "REMOVE FROM SCENE 1",
    82: "REMOVE FROM SCENE 2",
    83: "REMOVE FROM SCENE 3",
    84: "REMOVE FROM SCENE 4",
    85: "REMOVE FROM SCENE 5",
    86: "REMOVE FROM SCENE 6",
    87: "REMOVE FROM SCENE 7",
    88: "REMOVE FROM SCENE 8",
    89: "REMOVE FROM SCENE 9",
    90: "REMOVE FROM SCENE 10",
    91: "REMOVE FROM SCENE 11",
    92: "REMOVE FROM SCENE 12",
    93: "REMOVE FROM SCENE 13",
    94: "REMOVE FROM SCENE 14",
    95: "REMOVE FROM SCENE 15",
    96: "ADD TO GROUP 0",
    97: "ADD TO GROUP 1",
    98: "ADD TO GROUP 2",
    99: "ADD TO GROUP 3",
    100: "ADD TO GROUP 4",
    101: "ADD TO GROUP 5",
    102: "ADD TO GROUP 6",
    103: "ADD TO GROUP 7",
    104: "ADD TO GROUP 8",
    105: "ADD TO GROUP 9",
    106: "ADD TO GROUP 10",
    107: "ADD TO GROUP 11",
    108: "ADD TO GROUP 12",
    109: "ADD TO GROUP 13",
    110: "ADD TO GROUP 14",
    111: "ADD TO GROUP 15",
    112: "REMOVE FROM GROUP",
    128: "DTR AS SHORT",
    129: "ENABLE WRITE MEMORY",
    144: "QUERY STATUS",
    145: "QUERY CONTROL GEAR",
    146: "QUERY LAMP FAILURE",
    147: "QUERY LAMP POWER ON",
    148: "QUERY LIMIT ERROR",
    149: "QUERY RESET STATE",
    150: "QUERY MISSING SHORT",
    151: "QUERY VERSION",
    152: "QUERY DTR",
    153: "QUERY DEVICE TYPE",
    154: "QUERY PHYS MIN",
    155: "QUERY POWER FAILURE",
    156: "QUERY DTR1",
    157: "QUERY DTR2",
    158: "QUERY OPMODE",
    159: "QUERY LIGHTTYPE", // DALI-2
    160: "QUERY ACTUAL LEVEL",
    161: "QUERY MAX LEVEL",
    162: "QUERY MIN LEVEL",
    163: "QUERY POWER ON LEVEL",
    164: "QUERY FAIL LEVEL",
    165: "QUERY FADE SPEEDS",
    166: "QUERY SPECMODE",
    167: "QUERY NEXT DEVTYPE",
    168: "QUERY EXT FADE TIME",
    169: "QUERY CTRL GEAR FAIL", // DALI-2
    176: "QUERY SCENE_LEVEL",
    192: "QUERY GROUPS 0-7",
    193: "QUERY GROUPS 8-15",
    194: "QUERY ADDRH",
    195: "QUERY ADDRM",
    196: "QUERY ADDRL",
    197: "READ MEMORY LOCATION",
    255: "QUERY EXTENDED VERSION NUMBER"
};

const specialMap = {
    0: "TERMINATE",
    1: "SET DTR",
    2: "INITIALISE",
    3: "RANDOMISE",
    4: "COMPARE",
    5: "WITHDRAW",
    8: "SEARCHADDRH",
    9: "SEARCHADDRM",
    10: "SEARCHADDRL",
    11: "PROGRAM SHORT ADDRESS",
    12: "VERIFY SHORT ADDRESS",
    13: "QUERY SHORT ADDRESS",
    14: "PHYSICAL SELECTION"
};

const extendedMap = {
    0: "ENABLE DEVICE TYPE",
    1: "SET DTR1",
    2: "SET DTR2",
    3: "WRITE MEMORY LOCATION"
};

const extendedCmdMap = {
    8: {
        224: "SET TEMPORARY X-COORTINATE",
        225: "SET TEMPORARY Y-COORDINATE",
        226: "ACTIVATE",
        227: "X-COORDINATE STEP UP",
        228: "X-COORDINATE STEP DOWN",
        229: "Y-COORDINATE STEP UP",
        230: "Y-COORDINATE STEP DOWN",
        231: "SET TEMPORARY COLOUR TEMPERATURE",
        232: "COLOUR TEMPERATURE STEP COOLER",
        233: "COLOUR TEMPERATURE STEP WARMER",
        234: "SET TEMPORARY PRIMARY N DIMLEVEL",
        235: "SET TEMPORARY RGB DIMLEVEL",
        236: "SET TEMPORARY WAF DIMLEVEL",
        237: "SET TEMPORARY RGBWAF CONTROL",
        238: "COPY REPORT TO TEMPORARY",
        240: "STORE TY PRIMARY N",
        241: "STORE XY-COORDINATES PRIMARY N",
        242: "STORE COLOUR TEMPERATURE LIMIT",
        243: "STORE GEAR FEATURES/STATUS",
        245: "ASSIGN COLOUR TO LINKED CHANNEL",
        246: "START AUTO CALIBRATION",
        247: "QUERY GEAR FEATURES/STATUS",
        248: "QUERY COLOUR STATUS",
        249: "QUERY COLOUR TYPE FEATURES",
        250: "QUERY COLOUR VALUE",
        251: "QUERY RGBWAF CONTROL",
        252: "QUERY ASSIGNED COLOUR",
        255: "QUERY EXTENDED VERSION NUMBER"
    }
};

function arcToPercent(arc)
{
    if(arc == 0) return 0;
    let perc = Math.pow(10, ((arc-1) / (253/3.0)) - 1);
    perc = Math.round(perc * 100) / 100;
    return perc;
}

function connectWebSocket() {
    socket = new WebSocket("ws://" + window.location.hostname + "/");

    socket.onopen = function() {
        console.log("WebSocket verbunden");
        document.getElementById("status").textContent = "Verbunden";
        reconnectAttempts = 0;
    };

    socket.onerror = function(error) {
        console.error("WebSocket Fehler:", error);
        document.getElementById("status").textContent = "Fehler";
    };

    socket.onclose = function() {
        console.log("Verbindung verloren, versuche erneut...");
        document.getElementById("status").textContent = "Getrennt - Wiederverbindung...";
        let timeout = Math.min(5000 * Math.pow(2, reconnectAttempts), 60000);
        setTimeout(connectWebSocket, timeout);
        reconnectAttempts++;
    };

    socket.onmessage = function(event) {
        try {
            console.log(event.data);
            let logEntry = JSON.parse(event.data);
            if (logEntry.type === "daliMonitor" && logEntry.data) {
                addLogEntry(logEntry);
            }
            if(logEntry.type == "info") {
                offsetTimestamp = (Date.now() / 1000) - logEntry.timeSignature.timestamp;
            }
        } catch (error) {
            console.error("Fehler bei Nachricht:", error);
        }
    };
}

function addTestData()
{
    let fakeTimeStamp = 1027480.87;
    offsetTimestamp = (Date.now() / 1000) - fakeTimeStamp;

    var fake1 = new Date(fakeTimeStamp * 1000);
    var time = `${fake1.toLocaleTimeString()}.${fake1.getMilliseconds().toString().padStart(3, '0')}`;
    var date = fake1.toLocaleDateString();
    console.log(time, date);
    console.log(offsetTimestamp);
    
    var fake1 = new Date((fakeTimeStamp + offsetTimestamp) * 1000);
    time = `${fake1.toLocaleTimeString()}.${fake1.getMilliseconds().toString().padStart(3, '0')}`;
    date = fake1.toLocaleDateString();
    console.log(time, date);


    let data = { type: "daliMonitor",
        data: {
            tick_us: 86454,
            timestamp: fakeTimeStamp,
            bits: 16,
            data: [ 0, 145 ],
            line: 0,
            isEcho: true
        },
        timeSignature: {
            timestamp: 0,
            counter: 481
        }
    };

    addLogEntry(data); // ARC 5.1%
    data.data.bits = 8;
    data.data.data = [ 255 ];
    data.data.isEcho = false;
    data.data.timestamp += 0.015;
    addLogEntry(data); // ANSWER
    data.data.bits = 24;
    data.data.data = [ 255, 255, 0 ];
    data.data.timestamp += 0.017;
    addLogEntry(data); // 24bit unknown
    data.data.bits = 16;
    data.data.data = [ 0b01111111, 0];
    data.data.timestamp += 0.300;
    addLogEntry(data); // OFF A63
    data.data.data = [ 0b10011111, 0];
    data.data.timestamp += 0.584;
    addLogEntry(data); // OFF G15

    data.data.data = [ 0b10100101, 0];
    data.data.timestamp += 0.15;
    addLogEntry(data); // INIT all
    data.data.timestamp += 0.15;
    addLogEntry(data); // INIT all
    data.data.data = [ 0b10100111, 0];
    data.data.timestamp += 0.15;
    addLogEntry(data); // RAND
    data.data.timestamp += 0.15;
    addLogEntry(data); // RAND
    data.data.data = [0b11000001, 8];
    data.data.timestamp += 0.287;
    addLogEntry(data); // ENABLE DT8
    data.data.data = [0b11111111, 0b11100000];
    data.data.timestamp += 0.012;
    addLogEntry(data); // SET TEMP Y-COORD
    data.data.data = [0b11000001, 8];
    data.data.timestamp += 0.287;
    addLogEntry(data); // ENABLE DT8
    data.data.data = [0b11111111, 0b11100010];
    data.data.timestamp += 0.012;
    addLogEntry(data); // ACTIVATE
    data.data.data = [ 0b10011111, 0];
    data.data.timestamp += 0.584;
    addLogEntry(data); // OFF G15
    
    data.data.data = [ 254, 0];
    data.data.timestamp += 0.002;
    addLogEntry(data); // ARC 0 Bcast
    
    
    data.data.data = [ 254, 0];
    data.data.framingError = true;
    data.data.timestamp += 0.002;
    addLogEntry(data); // OFF G15
}

function parse8bit(frame)
{
    console.log("parsing 8bit");
    let response = {
        isEcho: frame.data.isEcho,
        line: frame.data.line,
        address: "",
        commandDescription: "Answer (" + (frame.data.data & 0xFF).toString(16).padStart(2, '0').toUpperCase() + ")",
        timestamp: frame.data.timestamp,
        error: frame.data.framingError ? frame.data.framingError : false
    }
    return response;
}

function parse16bit(frame)
{
    console.log("parsing 16bit", frame);
    let response = {
        isEcho: frame.data.isEcho,
        line: frame.data.line,
        timestamp: frame.data.timestamp,
        error: frame.data.framingError ? frame.data.framingError : false
    }

    response.commandDescription = "Unknown";

    // Decode the address and command from the 16-bit DALI message
    const firstByte = frame.data.data[0]; // Address byte
    const secondByte = frame.data.data[1]; // Command byte

    if(lastExtended >= 0)
    {
        response.address = "Extended";
        if(extendedCmdMap[lastExtended][secondByte] != undefined)
        {
            response.commandDescription = extendedCmdMap[lastExtended][secondByte];
        }
        else
            response.commandDescription = "Unknown DT" + lastExtended;

        lastExtended = -1;
        return response;
    }

    const frame_short = 0;
    const frame_group = 0b100;
    const frame_special = 0b101;
    const frame_extended = 0b110;
    const frame_broadcast = 0b111;

    let type  = frame_short;

    if((firstByte >> 7) === 1)
    {
        type = firstByte >> 5;
    }

    console.log("Type", type);


    if(type == frame_special)
    {
        response.address = "Special";
        let command = (firstByte >> 1) & 0b1111;
        if (specialMap[command] !== undefined) {
            response.commandDescription = specialMap[command];

            switch(command)
            {
                case 1: // SET DTR
                    response.commandDescription += " (" + secondByte.toString(16).padStart(2, '0').toUpperCase() + ")";
                    break;
                case 2: // Initialize
                    switch(secondByte)
                    {
                        case 0:
                            response.commandDescription += " (all)";
                            break;
                        case 255:
                            response.commandDescription += " (uninitialized)";
                            break;
                        default:
                            response.commandDescription += " (error)";
                            break;
                    }
                    break;

                case 8:  // SEARCHADDRH
                case 9:  // SEARCHADDRM
                case 10: // SEARCHADDRL
                    response.commandDescription += " (" + secondByte.toString(16).padStart(2, '0').toUpperCase() + ")";
                    break;

                case 11: // PROGRAM SHORT ADDRESS
                case 12: // VERIFY SHORT ADDRESS
                    response.commandDescription += " (" + (secondByte >> 1) + ")";
                    break;
            }
        }
    }
    else if(type == frame_extended)
    {
        response.address = "Extended";
        let command = (firstByte >> 1) & 0b1111;
        if(extendedMap[command] != undefined)
        {
            response.commandDescription = extendedMap[command];
            
            switch(command)
            {
                case 0: // ENABLE DEVICE TYPE
                    response.commandDescription += " (" + secondByte + ")";
                    lastExtended = secondByte;
                    break;
            }
        }
    }
    else {
        switch(type)
        {
            case frame_short:
                response.address = "A" +((firstByte >> 1) & 0b111111)
                break;

            case frame_group:
                response.address = "G" +((firstByte >> 1) & 0b1111)
                break;

            case frame_broadcast:
                response.address = "Bcast";
                break;

            default:
                response.address = "Unknown";
                break;
        }

        if(firstByte & 0b1)
        {
            if (commandMap[secondByte] !== undefined) {
                response.commandDescription = commandMap[secondByte];
            }
        }
        else
        {
            response.commandDescription = "ARC " + arcToPercent(secondByte) + "%";
        }
    }

    return response;
}

function addLogEntry(frame) {
    let tableBody = document.getElementById("logTable").getElementsByTagName("tbody")[0];
    let row = tableBody.insertRow(0);
    
    let frameResponse = {};

    if(frame.data.framingError || (frame.data.bits != 8 && frame.data.bits != 16 && frame.data.bits != 24 && frame.data.bits != 25))
    {
        frameResponse = {
            isEcho: frame.data.isEcho,
            line: frame.data.line,
            address: "Error",
            commandDescription: "Framing Error",
            timestamp: frame.data.timestamp,
            error: true
        };
    }
    else
    {
        switch(frame.data.bits)
        {
            case 8:
                lastExtended = -1;
                frameResponse = parse8bit(frame);
                break;

            case 16:
                frameResponse = parse16bit(frame);
                break;

            default:
                console.log("Unknown frame length", frame.data.bits);
                lastExtended = -1;
                frameResponse = {
                    isEcho: frame.data.isEcho,
                    line: frame.data.line,
                    address: "",
                    commandDescription: "Unknown",
                    timestamp: frame.data.timestamp,
                };
                break;

        }
    }

    console.log(frame.data.data);
    frameResponse.hexData = frame.data.data.map(byte => byte.toString(16).padStart(2, '0')).join('#').toUpperCase();
    console.log(frameResponse.hexData);
    
    var timestamp = new Date((frameResponse.timestamp + offsetTimestamp) * 1000);
    const time = `${timestamp.toLocaleTimeString()}.${timestamp.getMilliseconds().toString().padStart(3, '0')}`;
    const date = timestamp.toLocaleDateString();
    let delta = lastTimestamp ? timestamp - lastTimestamp : "";
    lastTimestamp = timestamp;

    console.log(frame, frameResponse);

    let rowClass = "";
    if (frameResponse.address.includes("Error")) rowClass = "error";
    else if (frameResponse.commandDescription.includes("Answer")) rowClass = "response";
    else if (frameResponse.address.includes("Special")) rowClass = "special";
    else if (frameResponse.address.includes("Extended")) rowClass = "extended";
    else if(frameResponse.commandDescription.includes("Unknown")) rowClass = "unknown";
    else rowClass = "command";
    row.className = rowClass;

    
    row.innerHTML = `
        <td>${frameResponse.isEcho ? "Gateway" : "Dali&nbsp;Bus"}</td>
        <td>L${frameResponse.line}</td>
        <td>${frameResponse.hexData.replaceAll("#", "&nbsp;")}</td>
        <td>${frameResponse.address}</td>
        <td>${frameResponse.commandDescription}</td>
        <td>${time}</td>
        <td>${date}</td>
        <td>${delta}</td>
    `;
    
    if (tableBody.rows.length > maxLogEntries) {
        tableBody.deleteRow(tableBody.rows.length - 1);
    }
}

function clearLogs() {
    let tableBody = document.getElementById("logTable").getElementsByTagName("tbody")[0];
    tableBody.innerHTML = "";
}

function filterLogs() {
    let searchText = document.getElementById("search").value.toLowerCase();
    let rows = document.getElementById("logTable").getElementsByTagName("tbody")[0].rows;
    for (let row of rows) {
        row.style.display = row.textContent.toLowerCase().includes(searchText) ? "" : "none";
    }
}

connectWebSocket();