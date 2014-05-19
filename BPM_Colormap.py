##########################################
# BUILD THE LIST DATA FOR METRONOME COLOR
#   Programatically generates color values 
# for a beats per minute range from 0 to 255.
# Slower is Red, and
# as we get faster, we "blue shift" the color.
# 
# Output from this file is copied/pasted
# into the metronome.h file directly.
#
#
# Author: Red Byer  
# Date:  4/2013
# 
# License: Free to use, no warranty implied.
#  Enjoy!
#
##########################################

redled=0
greenled=0
blulede=0
outputlist = []
maxbpm = 255
for bpm in range(0,maxbpm):  
    if (bpm < 60):
        redled=   0 + (4*bpm)
        greenled= 0
        blueled=  0
    elif (bpm < 120):
        redled=  240 - (4 * (bpm-60))
        greenled= 0  + (4 * (bpm-60))
        blueled=  0
    elif (bpm < 180):
        redled=   0  
        greenled= 240 - (4 * (bpm - 120))
        blueled=  0   + (4 * (bpm - 120))
    elif (bpm < 240):
        redled=   0   + (4 * (bpm - 180))
        greenled = 0
        blueled= 240
    elif (bpm < 246):
        redled   = 240
        greenled = 0+   (40 * (bpm - 239))
        blueled  = 240
    elif (bpm == 246):
        redled=255
        greenled=0
        blueled=0
    elif (bpm == 247):
        redled=0
        greenled=0
        blueled=255
    elif (bpm == 248):
        redled=0
        greenled=255
        blueled=0
    elif (bpm == 249):
        redled=255
        greenled=255
        blueled=255
    elif (bpm == 250):
        redled=255
        greenled=255
        blueled=0
    elif (bpm == 251):
        redled=255
        greenled=0
        blueled=255
    elif (bpm == 252):
        redled=150
        greenled=150
        blueled=150
    elif (bpm == 253):
        redled=255
        greenled=0
        blueled=0
    elif (bpm == 254):
        redled=0
        greenled=255
        blueled=0
    elif (bpm == 255):
        redled=0
        greenled=0
        blueled=255                
        #do some plaid
    else:
        pass
    temptuple = (redled, greenled, blueled)
    outputlist.append(temptuple)

####################################
# OUTPUT SECTION INTO C CODE
####################################
print "// LCD Backlight Table, based on BPMs"
print "// Generated via BPM_Colormap.py script...easier to edit that"
print """
/* Structure for defining a color */
struct color_24bits  {
                    uint8_t         red_value;     
                    uint8_t         green_value;
                    uint8_t         blue_value;
                    };"""
  
print "\n\n"

id = "bpmcolor"
bpm = 0   
for item in outputlist:
    print "const struct color_24bits " + id + str(bpm) + " PROGMEM = { " + str(item[0]) + ", " + str(item[1]) + ", " + str(item[2]) + " };"
    bpm += 1
    
    
print "\n\n"   
print "PROGMEM const struct color_24bits * BPM_COLOR_LIST[] = { "
bpm = 0
for bpm in range (0, maxbpm):
    print "                                  &" + id + str(bpm) + ", "
    
print "                                 };"
    
#    print str(bpm) + ": " + str(redled) + " " + str(greenled) + " " + str(blueled)
