var options = {};

var sendOptions = function (options) {
  Pebble.sendAppMessage(
    {"FACE_MODE": options.faceMode,
     "SHOW_GIFS": options.showGifs},
    function(e) {
      console.log("Sending settings data...");
    },
    function(e) {
      console.log("Settings feedback failed!");
    }
  );
};

var saveOptions = function (options) {
  localStorage.setItem("faceMode", options.faceMode);
  localStorage.setItem("showGifs", options.showGifs);
};

Pebble.addEventListener("ready",
  function(e) {
    options = {
      "faceMode": localStorage.getItem("faceMode") || "off",
      "showGifs": localStorage.getItem("showGifs") || "off"
    };
    sendOptions(options);
  }
);

Pebble.addEventListener("showConfiguration",
  function(e) {
    //Load the remote config page
    Pebble.openURL("http://bwinton.github.io/PipBoyFace/?options=" + encodeURIComponent(JSON.stringify(options)));    
  }
);

Pebble.addEventListener("webviewclosed",
  function(e) {
    options = JSON.parse(decodeURIComponent(e.response));
    console.log("Configuration window returned: " + JSON.stringify(options));

    saveOptions(options);
    sendOptions(options);
  }
);