function [x, y] = latlon2local(lat, lon, lat0, lon0)
    %{
    This function converts WGS84 coordinates (Lat, Lon) to a local
    tangent plane (x,y) in meters using an equirectangular projection.
    %}

    R = 6371000; % Earth's radius (in meters)
    
    x = R * deg2rad(lon - lon0) * cosd(lat0);
    
    y = R * deg2rad(lat - lat0);
end