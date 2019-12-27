const process = require('process');
const os = require('os');
const { exec } = require('child_process');
const gulp = require('gulp');
const clangFormat = require('gulp-clang-format');
const del = require('del');

const files =
[
	'src/**/*.cpp',
	'include/**/*.hpp',
	'test/src/**/*.cpp',
	'test/include/**/*.hpp'
];

gulp.task('lint', () =>
{
	const src = files;

	return gulp.src(src)
		.pipe(clangFormat.checkFormat('file', null, { verbose: true, fail: true }));
});

gulp.task('format', () =>
{
	const src = files;

	return gulp.src(src, { base: '.' })
		.pipe(clangFormat.format('file'))
		.pipe(gulp.dest('.'));
});

gulp.task('test', async () =>
{
	if (process.env.REBUILD === 'true')
	{
		console.log('[INFO] rebuilding project');

		await del('build', { force: true });

		let command = 'cmake . -Bbuild';

		command += ` -DLIBWEBRTC_INCLUDE_PATH:PATH=${process.env.PATH_TO_LIBWEBRTC_SOURCES}`;
		command += ` -DLIBWEBRTC_BINARY_PATH:PATH=${process.env.PATH_TO_LIBWEBRTC_BINARY}`;
		command += ' -DMEDIASOUPCLIENT_BUILD_TESTS:PATH="true"';
		command += ' -DCMAKE_CXX_FLAGS:PATH="-fvisibility=hidden"';

		await runCommand(command);
	}

	console.log('[INFO] compiling library');

	await runCommand('cmake --build build');

	console.log('[INFO] running test binary');

	let command;

	switch (os.type())
	{
		case 'Darwin':
			command = './build/test/test_mediasoupclient.app/Contents/MacOS/test_mediasoupclient';
			break;

		default:
			command = './build/test/test_mediasoupclient';
	}

	if (process.env.TEST_ARGS)
		command += ` ${process.env.TEST_ARGS}`;

	await runCommand(command);
});

async function runCommand(command)
{
	console.log('[INFO] running command:\n');
	console.log(command);
	console.log();

	const child = exec(command);

	child.stdout.on('data', (buffer) =>
	{
		for (const line of buffer.toString('utf8').split('\n'))
		{
			if (line)
				console.log(`[stdout] ${line}`);
		}
	});

	child.stderr.on('data', (buffer) =>
	{
		for (const line of buffer.toString('utf8').split('\n'))
		{
			if (line)
				console.error(`[stderr] ${line}`);
		}
	});

	await new Promise((resolve, reject) =>
	{
		child.on('exit', (code, signal) =>
		{
			console.log();

			if (!code && !signal)
				resolve();
			else
				reject(new Error(`child process exited [code:${code}, signal:${signal}]`));
		});
	});
}
