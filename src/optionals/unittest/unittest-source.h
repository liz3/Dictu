#define DICTU_UNITTEST_SOURCE "import Inspect;\n" \
"import System;\n" \
"import Future;\n" \
"\n" \
"abstract class UnitTest {\n" \
"    const METHOD_NAME_PADDING = '    ';\n" \
"    const RESULTS_PADDING     = '    ';\n" \
"    const ASSERTION_PADDING   = '         ';\n" \
"    var forceOnlyFailures   = false;\n" \
"    var forceExitOnFailure  = false;\n" \
"\n" \
"    init(var onlyFailures = false, var exitOnFailure = false) {\n" \
"        this.results = {\n" \
"            'passed': 0,\n" \
"            'failed': 0,\n" \
"            'skipped': 0\n" \
"        };\n" \
"    }\n" \
"\n" \
"    filterMethods() {\n" \
"        return this.methods().filter(def (method) => {\n" \
"            if (method.startsWith('test') and not method.endsWith('Provider') and not method.endsWith('_skipped')) {\n" \
"                return method;\n" \
"            }\n" \
"\n" \
"            if (method.endsWith('_skipped')) {\n" \
"                this.results['skipped'] += 1;\n" \
"            }\n" \
"        });\n" \
"    }\n" \
"\n" \
"    setUp() {\n" \
"\n" \
"    }\n" \
"\n" \
"    tearDown() {\n" \
"\n" \
"    }\n" \
"\n" \
"    async run() {\n" \
"        const methods = this.filterMethods();\n" \
"\n" \
"        print(Inspect.getFile(1));\n" \
"        for(var i = 0; i < methods.len(); i+=1) {\n" \
"            const method = methods[i];\n" \
"            print('{}{}()'.format(UnitTest.METHOD_NAME_PADDING, method));\n" \
"\n" \
"            const providerMethodName = '{}Provider'.format(method);\n" \
"\n" \
"            if (this.hasAttribute(providerMethodName)) {\n" \
"                const testValue = this.getAttribute(providerMethodName)();\n" \
"\n" \
"                if (type(testValue) == 'list') {\n" \
"                    for(var x = 0; x < testValue.len(); x+=1){\n" \
"                        this.setUp();\n" \
"                        await this.getAttribute(method)(val);\n" \
"                        this.tearDown();\n" \
"                    }\n" \
"                } else {\n" \
"                    this.setUp();\n" \
"                    await this.getAttribute(method)(testValue);\n" \
"                    this.tearDown();\n" \
"                }\n" \
"            } else {\n" \
"                this.setUp();\n" \
"                await this.getAttribute(method)();\n" \
"                this.tearDown();\n" \
"            }\n" \
"        }\n" \
"\n" \
"\n" \
"        print('\nResults:\n{}- {} assertion(s) were successful.\n{}- {} assertion(s) were failures.\n{}- {} method(s) were skipped.\n'.format(\n" \
"            UnitTest.RESULTS_PADDING,\n" \
"            this.results['passed'],\n" \
"            UnitTest.RESULTS_PADDING,\n" \
"            this.results['failed'],\n" \
"            UnitTest.RESULTS_PADDING,\n" \
"            this.results['skipped']\n" \
"        ));\n" \
"\n" \
"        if (this.results['failed'] > 0) {\n" \
"            System.exit(1);\n" \
"        }\n" \
"    }\n" \
"\n" \
"    printResult(success, errorMsg) {\n" \
"        if (success) {\n" \
"            this.results['passed'] += 1;\n" \
"\n" \
"            if (not (this.onlyFailures or this.forceOnlyFailures)) {\n" \
"                print('{}Success.'.format(UnitTest.ASSERTION_PADDING));\n" \
"            }\n" \
"        } else {\n" \
"            this.results['failed'] += 1;\n" \
"\n" \
"            print('{}Line: {} - {}'.format(UnitTest.ASSERTION_PADDING, Inspect.getLine(2), errorMsg));\n" \
"\n" \
"            if (this.exitOnFailure or this.forceExitOnFailure) {\n" \
"                System.exit(1);\n" \
"            }\n" \
"        }\n" \
"    }\n" \
"\n" \
"    assertEquals(value, expected) {\n" \
"        this.printResult(value == expected, 'Failure: {} is not equal to {}.'.format(value, expected));\n" \
"    }\n" \
"\n" \
"    assertNotEquals(value, expected) {\n" \
"        this.printResult(value != expected, 'Failure: {} is equal to {}.'.format(value, expected));\n" \
"    }\n" \
"\n" \
"    assertNil(value) {\n" \
"        this.printResult(value == nil, 'Failure: {} is not nil.'.format(value));\n" \
"    }\n" \
"\n" \
"    assertNotNil(value) {\n" \
"        this.printResult(value != nil, 'Failure: Should not be nil.');\n" \
"    }\n" \
"\n" \
"    assertType(value, expected) {\n" \
"        const valType = type(value);\n" \
"        this.printResult(valType == expected, 'Failure: {}({}) is not of type {}.'.format(value, valType, expected));\n" \
"    }\n" \
"\n" \
"    assertTruthy(value) {\n" \
"        this.printResult(value, 'Failure: {} is not Truthy.'.format(value));\n" \
"    }\n" \
"\n" \
"    assertFalsey(value) {\n" \
"        this.printResult(not value, 'Failure: {} is not Falsey.'.format(value));\n" \
"    }\n" \
"\n" \
"    assertSuccess(value) {\n" \
"        if (type(value) != 'result') {\n" \
"            this.printResult(false, 'Failure: {} is not a Result type.'.format(value));\n" \
"            return;\n" \
"        }\n" \
"\n" \
"        this.printResult(value.success(), 'Failure: {} is not a Result type in a success state.'.format(value));\n" \
"    }\n" \
"\n" \
"    assertError(value) {\n" \
"        if (type(value) != 'result') {\n" \
"            this.printResult(false, 'Failure: {} is not a Result type.'.format(value));\n" \
"            return;\n" \
"        }\n" \
"\n" \
"        this.printResult(not value.success(), 'Failure: {} is not a Result type in an error state.'.format(value));\n" \
"    }\n" \
"}\n" \

