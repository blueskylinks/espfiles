<?php
$filename = "data.json";

// Handle POST request (sender sends data)
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $input = json_decode(file_get_contents('php://input'), true);

    if ($input && isset($input['networkid'], $input['deviceid'], $input['vstate1'], $input['vstate2'])) {
        file_put_contents($filename, json_encode($input, JSON_PRETTY_PRINT));
        echo json_encode(["status" => "Data saved"]);
    } else {
        echo json_encode(["error" => "Invalid input"]);
    }
    exit;
}

// Handle GET request (receiver fetches data)
if ($_SERVER['REQUEST_METHOD'] === 'GET') {
    if (file_exists($filename)) {
        header('Content-Type: application/json');
        echo file_get_contents($filename);
    } else {
        echo json_encode(["error" => "No data found"]);
    }
    exit;
}
?>
