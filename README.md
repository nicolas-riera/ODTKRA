# Oculus Debug Tool Keep Rift Alive / ODTKRA
Uses the Oculus Debug Tool to keep your rift from going to sleep

This is a fork aimed to fix bugs and bring improvements to the tool.

## Instructions

- Download the .exe file from the releases

You have multiple ways of using it :
- Run the .exe after starting [Oculus Touch Steam Link](https://github.com/mm0zct/Oculus_Touch_Steam_Link) or [Driver4VR](https://store.steampowered.com/app/1366950/Driver4VR/) (depending on what software you are running)
- Rename the .exe to ```OculusDash.exe``` and put it in ```C:\Program Files\Meta Horizon\Support\oculus-dash\dash\bin```, or you can use [OculusDash-ODTKRA Switcher](https://github.com/nicolas-riera/OculusDash-ODTKRA_Switcher) that does it automatically.
  
Your headset will now not go to sleep, if it does, exit ODTKRA and launch it again.

## Optional Launch Options:

You can add launch options by right-clicking on a shortcut of ODTKRA.exe and going into properties and adding it in "target"

- --path ```ODTKRA.exe --path [Oculus Diagnostics Directory]```
    - Example: ```ODTKRA.exe --path "F:\Meta Horizon\Support\oculus-diagnostics"``` \
    <img width="466" height="237" alt="image" src="https://github.com/user-attachments/assets/9817e90a-4242-4a26-b9fa-a79e10ec7adc" />


