function [lat, lon] = local2latlon(x, y, lat0, lon0)
    %{
    This function converts local plane coordinates (x,y) in meters 
    to WGS84 (Lat, Lon) using an equirectangular projection..
    %}
    
    R = 6371000; % Earth's radius (in meters)
    
    lat = lat0 + rad2deg(y / R);
    
    lon = lon0 + rad2deg(x / (R * cosd(lat0)));
    
end