<?xml version = "1.0" encoding = "UTF-8"?>
<xsl:stylesheet version = "1.0" xmlns:xsl = "http://www.w3.org/1999/XSL/Transform">   
<xsl:template match = "/"> 
    <html> 
        <head>
	        <title>Test Report </title>
    	    <style>
	        	.header{
			        width: 75%;
			        margin: auto;
			        text-align: top;
			        height: 120px;
		        }
                
		        .filename{
                    height: 40px;			        
			        width: 100%;
		        }
		        .summary{
			        font-size: 12px;			        
			        width: 100%;
		        }
		        .summary table{ width: 100%; }		
		
                .summary table, .summary th, .summary td{
			        padding: 1px;
			        text-align: center;
		        }
		        .summary th{
			        background-color: #4CAF50;
			        color: white;
		        }
		        .summary tr:nth-child(even) {background-color: #c1c1c1;}

		        .log table{
			        width: 75%;
			        margin: auto;	
		        }
		        .log td, .log th { 
			        padding: 5px;
		        }
		        .log th{
			        background-color: #4CAF50;
    			    color: white;
		        }
		        .log tr:nth-child(even) {background-color: #c1c1c1;}
			.result, .elapsedtime{text-align: center;}
	        </style>
        </head>

        <body> 
            <h2 align = "center">Test Report</h2> 
	        <hr/>
	        <div class = "header">
	            <div class="filename">Filename: <xsl:value-of select = "TestFramework/Filename"/></div>
                <div class="summarytitle">Summary:</div>
	       	    <div class ="summary">
		            <table>
		                <tr>
			                <th>Type</th>
			                <th>Suites</th>
                            <th>Tests</th>
                            <th>Tests Not Found</th>
                            <th>Suites Not Found</th>
                            <th>Tests Passed</th>
                            <th>Tests Failed</th>
                            <th>Tests Timeout</th>
		                </tr>
		                <tr>
                            <th>Total</th>
			                <td><xsl:value-of select = "TestFramework/Summary/Suites"/></td>
                            <td><xsl:value-of select = "TestFramework/Summary/Tests"/></td>
                            <td><xsl:value-of select = "TestFramework/Summary/TestsNotFound"/></td>
                            <td><xsl:value-of select = "TestFramework/Summary/SuitesNotFound"/></td>
                            <td><xsl:value-of select = "TestFramework/Summary/TestsPassed"/></td>
                            <td><xsl:value-of select = "TestFramework/Summary/TestsFailed"/></td>
                            <td><xsl:value-of select = "TestFramework/Summary/TestsTimeout"/></td>
                        </tr>
	                </table>
	            </div>
	        </div>
	        <div> <hr/> </div> 
	        <div class="log">
	            <table>
                    <tr> 
                        <th>Test Suite Name</th> 
                        <th>Test Case Name</th> 
                        <th>Result</th>
		                <th>Elapsed Time(in sec)</th> 
                    </tr> 
				
                    <xsl:for-each select="TestFramework/TestSuite"> 
                        <tr> 
                            <td><xsl:value-of select = "Name"/></td> 
					        <td><xsl:value-of select = "TestCase/Name"/></td> 
                            <td class="result"><xsl:value-of select = "TestCase/Result"/></td>
		                    <td class="elapsedtime"><xsl:value-of select = "TestCase/ElapsedTime"/></td>
                        </tr> 
                    </xsl:for-each> 
					
                </table>
	        </div> 
         </body> 
      </html> 
   </xsl:template>  
</xsl:stylesheet>
