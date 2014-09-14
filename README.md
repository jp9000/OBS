# OBS-Help Github Page

## Hosting and updating the OBS help files
http://obsproject.com  
http://github.com/jp9000/OBS  
http://jp9000.github.io/OBS/

##General Information
New files have to be added to the _includes/menu.html.  
  
Links inside the help file directory can be 
created using:  
{{ site.baseurl }}  
For example:  
{{ site.baseurl }}/sources/windowcapture.html  
or  
{{ site.baseurl }}/assets/fileoutput.png

## Styling:
* < h4 > for headings with design
* < h3 > for sub-headings

Custom styles can be found in: 
* assets/style.css

Custom styles available in style.css:
* class="red_marker" for red text
* class="own_grid2" for two columns
* class="gray_border" for images if wanted

============================================
Created and maintained by [Jack0r](http://www.jack0r.com)