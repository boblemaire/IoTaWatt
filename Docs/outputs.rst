=======
Outputs
=======

Output channels provide useful values that are computed from 
input channel values using a calculator like interface. 
For instance, in a typical US installation, there are two MAIN circuits, 
the sum of the two is the total power into a panel.  
Its nice to know at a glance what that total is, 
but the two mains are measured separately using two input channels.  
We need a way to add them together to display the total usage.

Click Configure Outputs.

[Config Outputs]

This screen will list any outputs that you have already configured, 
and allow you to click "add" to create new ones.  
You may click "edit" on existing outputs to change or delete them.  
There is no practical limit to the number of outputs that you may create.  
The only requirement is that they be uniquely named.

So lets click ADD:

[Config Outputs Calculator]

This is the "calculator" interface that IoTaWatt uses to specify 
how to calculate an output using input channel values.  
It works just like the simple four function calculators we are all used to, 
with the exception that by pressing the "input" key, you can select that
an input channel value is to be inserted to the formula that you are creating. 
The resulting expression is evaluated left to right, 
except that calculations within parenthesis are evaluated before being used.

So lets make an output channel that combined two main inputs called Main_1 and Main_2. 
We enter the name Total_power in the Name box and hover over 
the "inputs" button of the calculator to see a list of the inputs.

[Config Output Total]

Select Main_1 from the list and it will appear in the calculator formula display.  
Next click on the + key, then repeat the input process selecting Main_2.

[Config Output Total]

Easy as that.  Now press save to return to the outputs list.  
Your new output should appear within a second or two.

[Config Outputs Total]

Now go back to the Channels Status screen and see that the new output channel 
is listed and indeed has a value that is the sum of the two inputs Main_1 and Main_2.

[Config Output Total]

Some other useful outputs would be:
* Power used in a solar PV system, calculated by adding the Solar inverter input 
to the (signed) Main input.  If for instance the inverter were putting out 4500 
watts and your Main(s) indicated an outflow represented as -3100 watts, 
local usage would be 1400 watts with 3100 watts exported.
* Where the Main(s) are monitored and selected circuits within the panel 
are also measured, you can create an output that shows the aggregate unmeasured usage 
by subtracting the measured inputs from the Mains (or local use if PV).

[Config Outputs Misc]
[Config Outputs Disp]