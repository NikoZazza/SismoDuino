<?php
include("timezone.php");
$ip = $_SERVER['REMOTE_ADDR'];
function fetch($host){
	if(function_exists('curl_init')){
    	$ch = curl_init();
		curl_setopt($ch, CURLOPT_URL, $host);
		curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
		curl_setopt($ch, CURLOPT_USERAGENT, 'geoP0lugin PHP Class v1.0');
		$response = curl_exec($ch);
		curl_close ($ch);
	}elseif(ini_get('allow_url_fopen')){
		$response = file_get_contents($host, 'r');	
	}else{
		return;
	}		
	return $response;
}	
$data = array();		
$response = fetch("http://www.geoplugin.net/php.gp?ip=$ip&base_currency=USD");		
$data = unserialize($response);
$timestamp = time();
$dt = new DateTime("now", new DateTimeZone(get_time_zone($data['geoplugin_countryCode'])));
$dt->setTimestamp($timestamp);
echo json_encode(array("timestamp" => date("U", $dt->format('U')), "lat" => $data['geoplugin_latitude'], "lon" => $data['geoplugin_longitude']));
?> 