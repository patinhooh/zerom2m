#
#	testSUB.py
#
#	(c) 2020 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	Modified by ZeroM2M Authors in 2026
#
#	Unit tests for SUB functionality & notifications
#

import unittest, sys
if '..' not in sys.path:
	sys.path.append('..')
from acmecse.etc.Types import NotificationEventType as NET, ResourceTypes as T, NotificationContentType, ResponseStatusCode as RC
from acmecse.etc.Types import Operation
from init import *

numberOfBatchNotifications = 5
durationForBatchNotifications = 2
durationForBatchNotificationsISO8601 = 'PT2S'

class TestSUB(unittest.TestCase):

	ae 				= None
	aeNoPoa 		= None
	aePoa 			= None
	originator 		= None
	originatorPoa 	= None
	cnt 			= None
	cntRI 			= None
	ae2URL 			= None
	aePOAURL		= None
	ae2Originator	= None
	excSub 			= None
	sub 			= None
	ts 				= None

	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def setUpClass(cls) -> None:
		global aeRN, aeURL, aeNoPoaURL, aePOAURL, cntRN, cntURL, subRN
		# Start notification server
		startNotificationServer()

		# look for notification server
		assert isNotificationServerRunning(), 'Notification server cannot be reached'

		testCaseStart('Setup TestSub')
		aeRN = uniqueRN('testAE')
		cntRN = uniqueRN('testCNT')
		subRN = uniqueRN('testSUB')
		aeURL = f'{cseURL}/{aeRN}'
		aeNoPoaURL = f'{cseURL}/{aeRN}NoPOA'
		aePOAURL = f'{cseURL}/{aeRN}POA'
		cntURL = f'{aeURL}/{cntRN}'
		# create other resources
		dct = 	{ 'm2m:ae' : {
					'rn'  : aeRN, 
					'api' : APPID,
				 	'rr'  : True,
				 	'srv' : [ RELEASEVERSION ],
				}}
		cls.ae, rsc = CREATE(cseURL, 'C', T.AE, dct)	# AE to work under
		assert rsc == RC.CREATED, 'cannot create parent AE'
		cls.originator = findXPath(cls.ae, 'm2m:ae/aei')

		dct = 	{ 'm2m:ae' : {
					'rn'  : f'{aeRN}NoPOA', 
					'api' : APPID,
					'rr'  : True,
					'srv' : [ RELEASEVERSION ]
				}}
		cls.aeNoPoa, rsc = CREATE(cseURL, 'C', T.AE, dct)	# AE to work under
		assert rsc == RC.CREATED, 'cannot create AE without poa'

		dct = 	{ 'm2m:ae' : {
					'rn'  : f'{aeRN}POA', 
					'api' : APPID,
				 	'rr'  : True,
				 	'srv' : [ RELEASEVERSION ],
					'poa' : [ NOTIFICATIONSERVER ],
				}}
		cls.aePoa, rsc = CREATE(cseURL, 'C', T.AE, dct)	# AE to work under
		assert rsc == RC.CREATED, 'cannot create parent AE with poa'
		cls.originatorPoa = findXPath(cls.aePoa, 'm2m:ae/aei')

		dct = 	{ 'm2m:cnt' : { 
					'rn'  : cntRN
				}}
		cls.cnt, rsc = CREATE(aeURL, cls.originator, T.CNT, dct)
		assert rsc == RC.CREATED, 'cannot create container'
		cls.cntRI = findXPath(cls.cnt, 'm2m:cnt/ri')
		cls.cntURL = cntURL

		# Add another AE URL
		cls.ae2URL = f'{cseURL}/{aeRN}2'
		cls.aePOAURL = aePOAURL

		testCaseEnd('Setup TestSub')


	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def tearDownClass(cls) -> None:
		if not isTearDownEnabled():
			stopNotificationServer()
			return
		testCaseStart('TearDown TestSub')
		# DELETE(aeURL, ORIGINATOR)	# Just delete the AE and everything below it. Ignore whether it exists or not
		# DELETE(f'{aeURL}NoPOA', ORIGINATOR)	# Just delete the NoPOA AE and everything below it. Ignore whether it exists or not
		# DELETE(f'{aeURL}POA', ORIGINATOR)	# Just delete the POA AE and everything below it. Ignore whether it exists or not
		# DELETE(f'{aeURL}2', ORIGINATOR)		# Just delete the AE and everything below it. Ignore whether it exists or not
		testCaseEnd('TearDown TestSub')
		stopNotificationServer()


	def setUp(self) -> None:
		testCaseStart(self._testMethodName)
	

	def tearDown(self) -> None:
		testCaseEnd(self._testMethodName)


	#########################################################################

	
	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUB(self) -> None:
		"""	CREATE <SUB> under <CNT>. """
		clearLastNotification()	# clear the notification first
		self.assertIsNotNone(TestSUB.ae)
		self.assertIsNotNone(TestSUB.cnt)
		dct = 	{ 'm2m:sub' : { 
					'rn' : subRN,
			        'enc': {
			            'net': [ NET.resourceUpdate, NET.createDirectChild ]
					},
					'nu': [ NOTIFICATIONSERVER ],
					'su': NOTIFICATIONSERVER
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.CREATED)
		global subURL
		subURL = f'{cntURL}/{findXPath(r, "m2m:sub/rn")}'

		# check notification
		lastNotification = getLastNotification()	# no delay! blocking
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/vrq'))
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/sur').endswith(findXPath(r, 'm2m:sub/ri')))
		self.assertEqual(findXPath(lastNotification, 'm2m:sgn/cr'), TestSUB.originator)	# CREATE notification must have the originator


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveSUB(self) -> None:
		"""	RETRIEVE <SUB>. """
		_, rsc = RETRIEVE(subURL, TestSUB.originator)
		self.assertEqual(rsc, RC.OK)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_retrieveSUBWithWrongOriginator(self) -> None:
		""" RETRIEVE <SUB> with wrong originator -> Fail"""
		_, rsc = RETRIEVE(subURL, 'Cwrong')
		self.assertEqual(rsc, RC.ORIGINATOR_HAS_NO_PRIVILEGE)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_attributesSUB(self) -> None:
		"""	Test <SUB> attributes. """
		r, rsc = RETRIEVE(subURL, TestSUB.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertEqual(findXPath(r, 'm2m:sub/ty'), T.SUB)
		self.assertEqual(findXPath(r, 'm2m:sub/pi'), findXPath(TestSUB.cnt,'m2m:cnt/ri'))
		self.assertEqual(findXPath(r, 'm2m:sub/rn'), subRN)
		self.assertIsNotNone(findXPath(r, 'm2m:sub/ct'))
		self.assertIsNotNone(findXPath(r, 'm2m:sub/lt'))
		self.assertIsNotNone(findXPath(r, 'm2m:sub/et'))
		self.assertIsNone(findXPath(r, 'm2m:sub/cr'))
		self.assertIsNotNone(findXPath(r, 'm2m:sub/enc/net'))
		self.assertIsInstance(findXPath(r, 'm2m:sub/enc/net'), list)
		self.assertEqual(len(findXPath(r, 'm2m:sub/enc/net')), 2)
		self.assertEqual(findXPath(r, 'm2m:sub/enc/net'), [1, 3])
		self.assertIsNotNone(findXPath(r, 'm2m:sub/nu'))
		self.assertIsInstance(findXPath(r, 'm2m:sub/nu'), list)
		self.assertEqual(findXPath(r, 'm2m:sub/nu'), [ NOTIFICATIONSERVER ])
		self.assertIsNotNone(findXPath(r, 'm2m:sub/nct'))
		self.assertIsInstance(findXPath(r, 'm2m:sub/nct'), int)
		self.assertEqual(findXPath(r, 'm2m:sub/nct'), 1)
		self.assertIsNone(findXPath(r, 'm2m:sub/nse'))
		self.assertIsNone(findXPath(r, 'm2m:sub/nsi'))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBWrong(self) -> None:
		""" CREATE <SUB> with unreachable notification URL -> Fail"""
		dct = 	{ 'm2m:sub' : { 
					'rn' : f'{subRN}Wrong',
			        'enc': {
			            'net': [ NET.resourceUpdate, NET.resourceDelete, NET.createDirectChild, NET.deleteDirectChild ]
        			},
        			'nu': [ NOTIFICATIONSERVERW ]
				}}
		_, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.SUBSCRIPTION_VERIFICATION_INITIATION_FAILED)
		
		# Try to retrieve subscription - Should fail
		_, rsc = RETRIEVE(f'{cntURL}/{subRN}Wrong', TestSUB.originator)
		self.assertEqual(rsc, RC.NOT_FOUND)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_addCIN2CNT(self) -> None:
		""" CREATE <CNI> to <CNT> -> Send notification withfull <CNI> resource"""
		dct = 	{ 'm2m:cin' : {
					'cnf' : 'text/plain:0',
					'con' : 'aValue'
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.CIN, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(r)
		self.assertIsNotNone(findXPath(r, 'm2m:cin/ri'))
		self.assertEqual(findXPath(r, 'm2m:cin/con'), 'aValue')

		# Check notification
		lastNotification = getLastNotification(wait = notificationDelay)
		self.assertIsNotNone(findXPath(lastNotification, 'm2m:sgn/nev/rep'))
		self.assertEqual(findXPath(lastNotification, 'm2m:sgn/nev/rep/m2m:cin/ty'), T.CIN)
		self.assertEqual(findXPath(lastNotification, 'm2m:sgn/nev/rep/m2m:cin/con'), 'aValue')
		self.assertIsNone(findXPath(lastNotification, 'm2m:sgn/cr'), lastNotification)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBModifedAttributesWrong(self) -> None:
		""" CREATE <SUB> to monitor only modified attributes and on CREATE of child resource -> Fail """
		dct = 	{ 'm2m:sub' : { 
					'rn' : subRN,
			        'enc': {
			            'net': [ NET.createDirectChild ]
        			},
        			'nu': [ NOTIFICATIONSERVER ],
					'su': NOTIFICATIONSERVER,
					'nct': NotificationContentType.modifiedAttributes
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBModifedAttributes(self) -> None:
		""" CREATE <SUB> to monitor only modified attributes and on UPDATE of child resource-> Send verification notification """
		dct = 	{ 'm2m:sub' : { 
					'rn' : subRN,
			        'enc': {
			            'net': [ NET.resourceUpdate ]
        			},
        			'nu': [ NOTIFICATIONSERVER ],
					'su': NOTIFICATIONSERVER,
					'nct': NotificationContentType.modifiedAttributes
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertEqual(findXPath(r, 'm2m:sub/nct'), NotificationContentType.modifiedAttributes)

		# Check notification
		lastNotification = getLastNotification()	# no delay! blocking
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/vrq'))
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/sur').endswith(findXPath(r, 'm2m:sub/ri')))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBRI(self) -> None:
		""" CREATE <SUB> to monitor the RI of new or updated resources -> Send verification notification"""
		dct = 	{ 'm2m:sub' : { 
					'rn' : subRN,
			        'enc': {
			            'net': [ NET.resourceUpdate, NET.createDirectChild ]
        			},
        			'nu': [ NOTIFICATIONSERVER ],
					'su': NOTIFICATIONSERVER,
					'nct': NotificationContentType.ri
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertEqual(findXPath(r, 'm2m:sub/nct'), NotificationContentType.ri)

		# Check notification
		lastNotification = getLastNotification()	# no delay! blocking
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/vrq'))
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/sur').endswith(findXPath(r, 'm2m:sub/ri')))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBForBatchNotificationNumber(self) -> None:
		""" CREATE <SUB> with batch notification set to number -> Send verification notification"""
		self.assertIsNotNone(TestSUB.ae)
		self.assertIsNotNone(TestSUB.cnt)
		subName = f'{subRN}_{self._testMethodName}_bnnum'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
			        'enc': {
			            'net': [ NET.resourceUpdate ]
					},
					'nu': [ NOTIFICATIONSERVER ],
					# No su! bc we want receive the last notification of a batch
					'bn': { 
						'num' : numberOfBatchNotifications
					}
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(findXPath(r, 'm2m:sub/bn'))
		self.assertEqual(findXPath(r, 'm2m:sub/bn/num'), numberOfBatchNotifications)
		self.assertIsNotNone(findXPath(r, 'm2m:sub/bn/dur'))
		self.assertGreater(findXPath(r, 'm2m:sub/bn/dur'), 0)

		# Check notification
		lastNotification = getLastNotification()	# no delay! blocking
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/vrq'))
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/sur').endswith(findXPath(r, 'm2m:sub/ri')))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBForBatchNotificationDuration(self) -> None:
		""" CREATE <SUB> with batch notification set to delay -> Send verification notification"""
		self.assertIsNotNone(TestSUB.ae)
		self.assertIsNotNone(TestSUB.cnt)
		subName = f'{subRN}_{self._testMethodName}_bndur'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
			        'enc': {
			            'net': [ NET.resourceUpdate ]
					},
					'nu': [ NOTIFICATIONSERVER ],
					# No su! bc we want receive the last notification of a batch
					'bn': { 
						'dur' : durationForBatchNotificationsISO8601
					}
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(findXPath(r, 'm2m:sub/bn'))
		self.assertIsNone(findXPath(r, 'm2m:sub/bn/num'))
		self.assertIsNotNone(findXPath(r, 'm2m:sub/bn/dur'))
		self.assertEqual(findXPath(r, 'm2m:sub/bn/dur'), durationForBatchNotificationsISO8601)

		# Check notification
		lastNotification = getLastNotification()	# no delay! blocking
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/vrq'))
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/sur').endswith(findXPath(r, 'm2m:sub/ri')))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBWithEncAtr(self) -> None:
		""" CREATE <SUB> to monitor resource update, only specific attribute -> Send verification notification"""
		self.assertIsNotNone(TestSUB.ae)
		self.assertIsNotNone(TestSUB.cnt)
		subName = f'{subRN}_{self._testMethodName}_encatr'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
			        'enc': {
			            'net': [ NET.resourceUpdate ],
			            'atr': ['lbl' ]
					},
					'nu': [ NOTIFICATIONSERVER ]
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(findXPath(r, 'm2m:sub/enc/atr'))
		self.assertEqual(findXPath(r, 'm2m:sub/enc/atr'), [ 'lbl' ])
		lastNotification = getLastNotification()	# no delay! blocking
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/vrq'))
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/sur').endswith(findXPath(r, 'm2m:sub/ri')))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBBatchNotificationNumberWithLn(self) -> None:
		""" CREATE <SUB> with batch notification set to number and lastNoitification=True -> Send verification notification"""
		subName = f'{subRN}_{self._testMethodName}_bnnln'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
			        'enc': {
			            'net': [ NET.resourceUpdate ],	# update resource
					},
					'ln': True,
					'nu': [ NOTIFICATIONSERVER ],
					'bn': { 
						'num' : numberOfBatchNotifications
					}
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(findXPath(r, 'm2m:sub/bn'))
		self.assertEqual(findXPath(r, 'm2m:sub/bn/num'), numberOfBatchNotifications)
		self.assertIsNotNone(findXPath(r, 'm2m:sub/ln'))
		self.assertEqual(findXPath(r, 'm2m:sub/ln'), True)
		lastNotification = getLastNotification()	# no delay! blocking
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/vrq'))
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/sur').endswith(findXPath(r, 'm2m:sub/ri')))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBWithEncChty(self) -> None:
		""" CREATE <SUB> to monitor ceration of child resources with type=container -> Send verification notification"""
		self.assertIsNotNone(TestSUB.ae)
		self.assertIsNotNone(TestSUB.cnt)
		subName = f'{subRN}_{self._testMethodName}_chty'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
			        'enc': {
			            'net': [ NET.createDirectChild ],	# create direct child resource
						'chty': [ T.CIN ] 	# only cin
					},
					'ln': True,
					'nu': [ NOTIFICATIONSERVER ]
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.CREATED)
		self.assertIsNotNone(findXPath(r, 'm2m:sub/enc/chty'))
		self.assertEqual(findXPath(r, 'm2m:sub/enc/chty'), [ T.CIN ])
		lastNotification = getLastNotification()	# no delay! blocking
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/vrq'))
		self.assertTrue(findXPath(lastNotification, 'm2m:sgn/sur').endswith(findXPath(r, 'm2m:sub/ri')))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAESUBwithOriginatorPOA(self) -> None:
		""" CREATE new <AE> and <SUB>. nu=poa of new originator -> NO verification request"""
		# create a second AE
		aeName = f'{aeRN}2_{self._testMethodName}'
		dct = 	{ 'm2m:ae' : {
			'rn'  : aeName, 
			'api' : APPID,
			'rr'  : True,
			'srv' : [ RELEASEVERSION ],
			'poa' : [ NOTIFICATIONSERVER ]
		}}
		ae, rsc = CREATE(cseURL, 'C', T.AE, dct)
		self.assertEqual(rsc, RC.CREATED)
		TestSUB.ae2Originator = findXPath(ae, 'm2m:ae/aei')
		TestSUB.ae2URL = f'{cseURL}/{aeName}'

		# Create the sub
		clearLastNotification()	# clear the notification first
		subName = f'{subRN}_{self._testMethodName}'
		dct = 	{ 'm2m:sub' : { 
				'rn' : subName,
			        'enc': {
			            'net': [ NET.resourceUpdate, NET.createDirectChild ]
					},
					'nu': [ TestSUB.ae2Originator ],
					'su': NOTIFICATIONSERVER
				}}
		r, rsc = CREATE(TestSUB.ae2URL, TestSUB.ae2Originator, T.SUB, dct)
		self.assertEqual(rsc, RC.CREATED, r)
		lastNotification = getLastNotification()	# no delay! blocking
		self.assertIsNone(findXPath(lastNotification, 'm2m:sgn/vrq'))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createAESUBwithOriginatorPOANotReachable(self) -> None:
		""" CREATE new <AE>, <SUB>, <CNT>. Not request reachable, no notification request"""
		# create a second AE
		aeName = f'{aeRN}2_{self._testMethodName}'
		dct = 	{ 'm2m:ae' : {
			'rn'  : aeName, 
			'api' : APPID,
			'rr'  : False,
			'srv' : [ RELEASEVERSION ],
			'poa' : [ NOTIFICATIONSERVER ]
		}}
		ae, rsc = CREATE(cseURL, 'C', T.AE, dct)
		self.assertEqual(rsc, RC.CREATED)
		TestSUB.ae2Originator = findXPath(ae, 'm2m:ae/aei')
		TestSUB.ae2URL = f'{cseURL}/{aeName}'

		# Create the sub
		clearLastNotification()	# clear the notification first
		subName = f'{subRN}_{self._testMethodName}'
		dct = 	{ 'm2m:sub' : { 
				'rn' : subName,
			        'enc': {
			            'net': [ NET.resourceUpdate, NET.createDirectChild ]
					},
					'nu': [ TestSUB.ae2Originator ],
					'su': NOTIFICATIONSERVER
				}}
		r, rsc = CREATE(TestSUB.ae2URL, TestSUB.ae2Originator, T.SUB, dct)
		self.assertEqual(rsc, RC.CREATED, r)
		lastNotification = getLastNotification()	# no delay! blocking
		self.assertIsNone(findXPath(lastNotification, 'm2m:sgn/vrq'))

		# Create the CNT
		clearLastNotification()	# clear the notification first
		cntName = f'{cntRN}_{self._testMethodName}'
		dct = 	{ 'm2m:cnt' : { 
				'rn'  : cntName
				}}
		TestSUB.cnt, rsc = CREATE(TestSUB.ae2URL, TestSUB.ae2Originator, T.CNT, dct)
		self.assertEqual(rsc, RC.CREATED)
		lastNotification = getLastNotification()
		self.assertIsNone(lastNotification)		# this must have NOT caused a notification via the poa (because not request reachable)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createCNTwithOriginatorPOA(self) -> None:
		""" CREATE <CNT> -> Send notification via AE.poa """
		# Create a fresh AE with POA and subscribe the AE (make test independent)
		aeName = f'{aeRN}2_{self._testMethodName}_local'
		dct = 	{ 'm2m:ae' : {
				'rn'  : aeName,
				'api' : APPID,
				'rr'  : True,
				'srv' : [ RELEASEVERSION ],
				'poa' : [ NOTIFICATIONSERVER ]
			}}
		ae, rsc = CREATE(cseURL, 'C', T.AE, dct)
		self.assertEqual(rsc, RC.CREATED)
		localOriginator = findXPath(ae, 'm2m:ae/aei')
		localURL = f'{cseURL}/{aeName}'

		# Create subscription for this AE that delivers to the AE originator (POA)
		clearLastNotification()   # clear the notification first
		subName = f'{subRN}_{self._testMethodName}_local'
		dct = 	{ 'm2m:sub' : {
				'rn' : subName,
				'enc': {
					'net': [ NET.createDirectChild ]
				},
				'nu': [ localOriginator ],
				'su': NOTIFICATIONSERVER
			}}
		r, rsc = CREATE(localURL, localOriginator, T.SUB, dct)
		self.assertEqual(rsc, RC.CREATED, r)

		clearLastNotification()	# clear the notification first
		cntName = f'{cntRN}_{self._testMethodName}'
		dct = 	{ 'm2m:cnt' : { 
				'rn'  : cntName
				}}
		TestSUB.cnt, rsc = CREATE(localURL, localOriginator, T.CNT, dct)
		self.assertEqual(rsc, RC.CREATED)
		lastNotification = getLastNotification(wait = notificationDelay)
		self.assertIsNotNone(lastNotification)		# this must have caused a notification via the poa
		self.assertIsNotNone(findXPath(lastNotification, 'm2m:sgn/nev/rep/m2m:cnt/rn'))
		self.assertEqual(findXPath(lastNotification, 'm2m:sgn/nev/rep/m2m:cnt/rn'), cntName)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBwithUnknownPoa(self) -> None:
		""" CREATE new <SUB> with NU to not-existing POA -> Fail """
		# Create the sub
		clearLastNotification()	# clear the notification first
		dct = 	{ 'm2m:sub' : { 
					'rn' : subRN+'NOPOA',
			        'enc': {
			            'net': [ NET.resourceUpdate, NET.createDirectChild ]
					},
					'nu': [ findXPath(TestSUB.aeNoPoa, 'm2m:ae/ri') ]	# this ae has no poa
				}}
		TestSUB.excSub, rsc = CREATE(aeURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.SUBSCRIPTION_VERIFICATION_INITIATION_FAILED)
		
	
	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBWithCreatorWrong(self) -> None:
		""" CREATE <SUB> with creator attribute (wrong) -> Fail """
		dct = 	{ 'm2m:sub' : { 
					'rn' : f'{subRN}_{self._testMethodName}_creatorwrong',
					'nu': [NOTIFICATIONSERVER ],
					'cr' : 'wrong',
				}}
		r, rsc = CREATE(aeURL, TestSUB.originator, T.SUB, dct)				# Not allowed
		self.assertEqual(rsc, RC.BAD_REQUEST)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBWithCreatorNull(self) -> None:
		""" UPDATE <SUB> with creator attribute set to Null """#

		clearLastNotification()
		dct = 	{ 'm2m:sub' : { 
					'rn' : f'{subRN}_{self._testMethodName}_creatornull',
					'nu': [NOTIFICATIONSERVER ],
					'cr' : None,
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)	
		self.assertEqual(rsc, RC.CREATED)
		self.assertEqual(findXPath(r, 'm2m:sub/cr'), TestSUB.originator)	# Creator should now be set to originator

		# Check the latest verification Request
		notification = getLastNotification()	# no delay! blocking
		self.assertIsNotNone(notification)
		self.assertIsNotNone(findXPath(notification, 'm2m:sgn/vrq'), notification)
		self.assertIsNotNone(findXPath(notification, 'm2m:sgn/cr'), notification)
		self.assertEqual(findXPath(notification, 'm2m:sgn/cr'), TestSUB.originator, notification)

		# Check whether creator is there in a RETRIEVE
		r, rsc = RETRIEVE(f'{cntURL}/{findXPath(r, "m2m:sub/rn")}', TestSUB.originator)
		self.assertEqual(rsc, RC.OK)
		self.assertEqual(findXPath(r, 'm2m:sub/cr'), TestSUB.originator)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBForMissingDataFail(self) -> None:
		""" CREATE <SUB> for Missing Data under <CNT> -> Fail"""
		dct = 	{ 'm2m:sub' : { 
					'nu': [NOTIFICATIONSERVER ],
			        'enc': {
			            'net': [ NET.reportOnGeneratedMissingDataPoints ]
					},				
				}}
		r, rsc = CREATE(cntURL, TestSUB.originator, T.SUB, dct)	
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBWithWrongCHTYFail(self) -> None:
		""" CREATE <SUB> with wrong CHTY -> Fail"""

		dct = 	{ 'm2m:sub' : { 
					'nu': [NOTIFICATIONSERVER ],
			        'enc': {
			            'net':  [ NET.deleteDirectChild ],
						'chty': [ T.AE ]
					},
				}}
		TestSUB.sub, rsc = CREATE(f'{aeURL}', TestSUB.originator, T.SUB, dct)	
		self.assertEqual(rsc, RC.BAD_REQUEST, TestSUB.sub)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBWithStructuredNu(self) -> None:
		""" CREATE <SUB> with structured ID in NU"""
		clearLastNotification()
		subName = f'{subRN}_{self._testMethodName}_structnu'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
					'nu': [ f'{CSERN}/{aeRN}POA' ],
			        'enc': {
			            'net':  [ NET.createDirectChild ],
					},
				}}
		r, rsc = CREATE(self.aePOAURL, TestSUB.originatorPoa, T.SUB, dct)	
		self.assertEqual(rsc, RC.CREATED, r)
		self.assertIsNone(getLastNotification(wait = notificationDelay))


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSubBlockingUpdateWrongNUFail(self) -> None:
		""" CREATE <SUB> for blocking Update with wrong NU -> Fail"""
		subName = f'{subRN}_{self._testMethodName}_wrongnu'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
					'nu': [ NOTIFICATIONSERVERW ],
			        'enc': {
			            'net':  [ NET.blockingUpdate ],
					},
				}}
		r, rsc = CREATE(aeURL, TestSUB.originator, T.SUB, dct)	
		self.assertEqual(rsc, RC.SUBSCRIPTION_VERIFICATION_INITIATION_FAILED, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSubBlockingUpdateMultipleNUFail(self) -> None:
		""" CREATE <SUB> for blocking Update with multiple NU -> Fail"""
		subName = f'{subRN}_{self._testMethodName}_multinu'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
					'nu': [ f'{CSERN}/{aeRN}POA', NOTIFICATIONSERVER ],
			        'enc': {
			            'net':  [ NET.blockingUpdate ],
					},
				}}
		r, rsc = CREATE(aeURL, TestSUB.originator, T.SUB, dct)	
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSubBlockingUpdateDisallowedAttributeFail(self) -> None:
		""" CREATE <SUB> for blocking Update with disallowed resource attribute -> Fail"""
		subName = f'{subRN}_{self._testMethodName}_badattr'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
					'nu': [ f'{CSERN}/{aeRN}POA' ],
			        'enc': {
			            'net':  [ NET.blockingUpdate ],
					},
					'bn': { 
						'num' : numberOfBatchNotifications
					}
				}}
		r, rsc = CREATE(aeURL, TestSUB.originator, T.SUB, dct)	
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSubBlockingUpdateDisallowedENCAttributeFail(self) -> None:
		""" CREATE <SUB> for blocking Update with disallowed "enc" attribute -> Fail"""
		subName = f'{subRN}_{self._testMethodName}_badenc'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
					'nu': [ f'{CSERN}/{aeRN}POA' ],
			        'enc': {
			            'net':  [ NET.blockingUpdate ],
						'sza': 0,
					},
				}}
		r, rsc = CREATE(aeURL, TestSUB.originator, T.SUB, dct)	
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSubBlockingUpdateDisallowedNCTvaluel(self) -> None:
		""" CREATE <SUB> for blocking Update with disallowed "nct" value -> Fail"""
		subName = f'{subRN}_{self._testMethodName}_badnct'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
					'nu': [ f'{CSERN}/{aeRN}POA' ],
			        'enc': {
			            'net':  [ NET.blockingUpdate ],
					},
					'nct': NotificationContentType.allAttributes,
				}}
		r, rsc = CREATE(aeURL, TestSUB.originator, T.SUB, dct)	
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSubBlockingUpdate(self) -> None:
		""" CREATE <SUB> for blocking Update"""
		subName = f'{subRN}_{self._testMethodName}_block'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
					'nu': [ f'{CSERN}/{aeRN}POA' ],
			        'enc': {
			            'net': [ NET.blockingUpdate ],
						'atr': [ 'lbl' ],
					},
					'nct': NotificationContentType.modifiedAttributes,
				}}
		r, rsc = CREATE(self.aePOAURL, TestSUB.originatorPoa, T.SUB, dct)	
		self.assertEqual(rsc, RC.CREATED, r)


	#########################################################################
	#
	#	NotificationStats tests
	#

#
#	Test defaults
#

	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createSUBnoNCTwrongNETFail(self) -> None:
		""" CREATE <sub> with no NCT and conflicting NCT -> Fail """
		subName = f'{subRN}_{self._testMethodName}_nonct'
		dct = 	{ 'm2m:sub' : { 
					'rn' : subName,
					'enc': {
						'net': [ NET.resourceUpdate, NET.blockingUpdate ]
					},
					'nu': [ NOTIFICATIONSERVER ],
					'su': NOTIFICATIONSERVER,
					'nse': True
				}}
		r, rsc = CREATE(aeURL, TestSUB.originator, T.SUB, dct)
		self.assertEqual(rsc, RC.BAD_REQUEST, r)


# TODO check different NET's (ae->cnt->sub, add cnt to cnt)
# TODO add further checks for operationMonitor and NCT values


def run(testFailFast:bool) -> TestResult:

	# Assign tests
	suite = unittest.TestSuite()
	addTests(suite, TestSUB, [
		'test_createSUB',
		'test_retrieveSUB',
		'test_retrieveSUBWithWrongOriginator',
		'test_attributesSUB',

		'test_createSUBWrong',
		'test_addCIN2CNT',
		'test_createSUBForBatchNotificationNumber',
		'test_createSUBForBatchNotificationDuration',
		'test_createSUBWithEncAtr',
		'test_createSUBBatchNotificationNumberWithLn',
		'test_createSUBWithEncChty',
		'test_createAESUBwithOriginatorPOA',
		'test_createAESUBwithOriginatorPOANotReachable',
		'test_createCNTwithOriginatorPOA',

		'test_createSUBWithCreatorWrong',
		'test_createSUBWithCreatorNull',

		# Missing Data
		'test_createSUBForMissingDataFail',

		# Wrong CHTY
		'test_createSUBWithWrongCHTYFail',

		# Structured NU
		'test_createSUBWithStructuredNu',

		# Create-only subscription modes
		'test_createSUBnoNCTwrongNETFail',
		'test_createSubBlockingUpdateWrongNUFail',
		'test_createSubBlockingUpdateMultipleNUFail',
		'test_createSubBlockingUpdateDisallowedAttributeFail',
		'test_createSubBlockingUpdateDisallowedENCAttributeFail',
		'test_createSubBlockingUpdateDisallowedNCTvaluel',
		'test_createSubBlockingUpdate',
	])

	# Run tests
	result = unittest.TextTestRunner(verbosity=testVerbosity, failfast=testFailFast).run(suite)
	printResult(result)
	return result.testsRun, len(result.errors + result.failures), len(result.skipped), getSleepTimeCount()


if __name__ == '__main__':
	r, errors, s, t = run(True)
	sys.exit(errors)
